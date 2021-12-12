/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/rest.h"

#include <algorithm>
#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/json/parser.h"

#include "roq/core/metrics/factory.h"

#include "roq/okex/flags.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static const auto NAME = "rest"sv;

static const auto SUPPORTS = utils::Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

static const auto ALLOW_PIPELINING = true;

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

Rest::Rest(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
          *this,
          context,
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          core::URI(Flags::rest_uri()),
          ROQ_PACKAGE_NAME,
          core::http::Connection::KEEP_ALIVE,
          ALLOW_PIPELINING,
          Flags::rest_request_timeout(),
          Flags::rest_ping_freq(),
          Flags::rest_ping_path()),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .status = create_metrics(name_, "status"sv),
          .status_ack = create_metrics(name_, "status_ack"sv),
          .instruments_spot = create_metrics(name_, "instruments_spot"sv),
          .instruments_margin = create_metrics(name_, "instruments_margin"sv),
          .instruments_swap = create_metrics(name_, "instruments_swap"sv),
          .instruments_futures = create_metrics(name_, "instruments_futures"sv),
          .instruments_option = create_metrics(name_, "instruments_option"sv),
          .instruments_ack = create_metrics(name_, "instruments_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void Rest::operator()(const Event<Start> &) {
  connection_.start();
}

void Rest::operator()(const Event<Stop> &) {
  connection_.stop();
}

void Rest::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
  if (ready())
    check_request_queue(now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.instruments_spot, metrics::PROFILE)
      .write(profile_.instruments_margin, metrics::PROFILE)
      .write(profile_.instruments_swap, metrics::PROFILE)
      .write(profile_.instruments_futures, metrics::PROFILE)
      .write(profile_.instruments_option, metrics::PROFILE)
      .write(profile_.instruments_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void Rest::operator()(const core::web::Client::Connected &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void Rest::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void Rest::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

uint32_t Rest::download(RestState state) {
  switch (state) {
    case RestState::UNDEFINED:
      assert(false);
      break;
    case RestState::STATUS:
      get_status();
      return 1;
    case RestState::INSTRUMENTS_SPOT:
      get_instruments_spot();
      return 1;
    case RestState::INSTRUMENTS_MARGIN:
      get_instruments_margin();
      return 1;
    case RestState::INSTRUMENTS_SWAP:
      get_instruments_swap();
      return 1;
    case RestState::INSTRUMENTS_FUTURES:
      get_instruments_futures();
      return 1;
    case RestState::INSTRUMENTS_OPTION:
      get_instruments_option();
      return 1;
    case RestState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

// status

void Rest::get_status() {
  profile_.status([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/system/status"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "status"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_status_ack(event, sequence);
        });
  });
}

void Rest::get_status_ack(const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.status_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = RestState::STATUS;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto status_ = core::json::Parser::create<json::Status>(body, buffer);
      server::Trace event(trace_info, status_);
      (*this)(event);
      download_.check_relaxed(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::Status> &event) {
  auto &[trace_info, status] = event;
  log::info<2>("status={}"sv, status);
}

// instruments

void Rest::get_instruments_spot() {
  profile_.instruments_spot([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/public/instruments?instType=SPOT"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "instruments"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_instruments_ack(event, sequence, RestState::INSTRUMENTS_SPOT);
        });
  });
}

void Rest::get_instruments_margin() {
  profile_.instruments_margin([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/public/instruments?instType=MARGIN"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "instruments"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_instruments_ack(event, sequence, RestState::INSTRUMENTS_MARGIN);
        });
  });
}

void Rest::get_instruments_swap() {
  profile_.instruments_swap([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/public/instruments?instType=SWAP"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "instruments"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_instruments_ack(event, sequence, RestState::INSTRUMENTS_SWAP);
        });
  });
}

void Rest::get_instruments_futures() {
  profile_.instruments_futures([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/public/instruments?instType=FUTURES"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "instruments"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_instruments_ack(event, sequence, RestState::INSTRUMENTS_FUTURES);
        });
  });
}

void Rest::get_instruments_option() {
  profile_.instruments_option([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/public/instruments?instType=OPTION"sv;
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "instruments"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_instruments_ack(event, sequence, RestState::INSTRUMENTS_OPTION);
        });
  });
}

void Rest::get_instruments_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence, RestState state) {
  profile_.instruments_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto tickers = core::json::Parser::create<json::Instruments>(body, buffer);
      server::Trace event(trace_info, tickers);
      (*this)(event);
      download_.check_relaxed(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::Instruments> &event) {
  auto &[trace_info, instruments] = event;
  log::info<2>("instruments={}"sv, instruments);
  std::vector<std::string> symbols;
  symbols.reserve(std::size(instruments.data));
  size_t counter = {};
  for (auto &item : instruments.data) {
    log::info<2>("item={}"sv, item);
    auto symbol = item.inst_id;
    if (shared_.discard_symbol(symbol))
      continue;
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols.emplace_back(symbol);
    ++counter;
    ReferenceData reference_data{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = symbol,
        .description = {},
        .security_type = {},  // XXX HANS item.inst_type
        .base_currency = item.base_ccy,
        .quote_currency = item.quote_ccy,
        .commission_currency = {},
        .tick_size = item.tick_sz,
        .multiplier = item.ct_mult,
        .min_trade_vol = item.min_sz,
        .max_trade_vol = NaN,
        .trade_vol_step_size = NaN,
        .option_type = {},  // XXX HANS item.opt_type,
        .strike_currency = {},
        .strike_price = item.stk,
        .underlying = item.uly,
        .time_zone = {},
        .issue_date = utils::safe_cast(item.list_time),
        .settlement_date = {},
        .expiry_datetime = utils::safe_cast(item.exp_time),
        .expiry_datetime_utc = utils::safe_cast(item.exp_time),
    };
    server::create_trace_and_dispatch(handler_, trace_info, reference_data, true);
    MarketStatus market_status{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.inst_id,
        .trading_status = {},  // XXX HANS item.state
    };
    server::create_trace_and_dispatch(handler_, trace_info, market_status, true);
  }
  log::info("Instruments {} / {}"sv, counter, std::size(instruments.data));
  if (!std::empty(symbols)) {
    SymbolsUpdate symbols_update{
        .symbols = symbols,
    };
    handler_(symbols_update);
  }
}

// queue

void Rest::check_request_queue(std::chrono::nanoseconds now) {
  while (!std::empty(shared_.request_queue)) {
    auto &tmp = shared_.request_queue.front();
    if (now < tmp.first)
      break;
    if (shared_.can_request(now, [&]() {
          /*
          auto &symbol = tmp.second;
          log::debug(R"(Requesting order book snapshot symbol="{}")"sv, symbol);
          get_order_book(symbol);
          shared_.request_queue.pop_front();
          */
        })) {
    } else {
      return;
    }
  }
}

}  // namespace okex
}  // namespace roq
