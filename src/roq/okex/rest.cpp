/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/rest.h"

#include <algorithm>
#include <utility>

#include "roq/utils/mask.h"
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

void emplace(MBPUpdate &result, double price, double size) {
  new (&result) MBPUpdate{
      .price = price,
      .quantity = size,
      .implied_quantity = NaN,
      .price_level = {},
      .number_of_orders = {},
  };
}
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
          .public_token = create_metrics(name_, "public_token"sv),
          .public_token_ack = create_metrics(name_, "public_token_ack"sv),
          .currencies = create_metrics(name_, "currencies"sv),
          .currencies_ack = create_metrics(name_, "currencies_ack"sv),
          .symbols = create_metrics(name_, "symbols"sv),
          .symbols_ack = create_metrics(name_, "symbols_ack"sv),
          .order_book = create_metrics(name_, "order_book"sv),
          .order_book_ack = create_metrics(name_, "order_book_ack"sv),
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
  if (refresh_token_.count() && refresh_token_ < now) {
    refresh_token_ = now + Flags::rest_token_refresh_freq();
    get_public_token();
  }
  if (ready())
    check_request_queue(now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.public_token, metrics::PROFILE)
      .write(profile_.public_token_ack, metrics::PROFILE)
      .write(profile_.currencies, metrics::PROFILE)
      .write(profile_.currencies_ack, metrics::PROFILE)
      .write(profile_.symbols, metrics::PROFILE)
      .write(profile_.symbols_ack, metrics::PROFILE)
      .write(profile_.order_book, metrics::PROFILE)
      .write(profile_.order_book_ack, metrics::PROFILE)
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
  refresh_token_ = {};
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
    case RestState::PUBLIC_TOKEN:
      get_public_token();
      return 1;
    case RestState::CURRENCIES:
      get_currencies();
      return 1;
    case RestState::SYMBOLS:
      get_symbols();
      return 1;
    case RestState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

// public-token

void Rest::get_public_token() {
  profile_.public_token([&]() {
    auto method = core::http::Method::POST;
    auto path = "/api/v1/bullet-public"sv;
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
        "public_token"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_public_token_ack(event, sequence);
        });
  });
}

void Rest::get_public_token_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.public_token_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = RestState::PUBLIC_TOKEN;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto token = core::json::Parser::create<json::Token>(body, buffer);
      server::Trace event(trace_info, token);
      (*this)(event);
      download_.check_relaxed(state);
      auto now = core::get_system_clock();
      refresh_token_ = now + Flags::rest_token_refresh_freq();
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::Token> &event) {
  auto &[trace_info, token] = event;
  log::info<2>("token={}"sv, token);
  if (std::empty(token.data.instance_servers))
    log::fatal("Unexpected: no instance servers"sv);
  auto &instance_server = token.data.instance_servers[0];
  auto query = fmt::format("?token={}"sv, token.data.token);
  PublicToken const public_token{
      .uri = instance_server.endpoint,
      .query = query,
      .ping_frequency = instance_server.ping_interval,
  };
  if (public_token.ping_frequency.count() == 0)
    log::fatal("Unexpected: ping_interval={}"sv, instance_server.ping_interval);
  handler_(public_token);
}

// currencies

void Rest::get_currencies() {
  profile_.currencies([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v1/currencies"sv;
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
        "currencies"sv,
        request,
        [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_currencies_ack(event, sequence);
        });
  });
}

void Rest::get_currencies_ack(const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.currencies_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = RestState::CURRENCIES;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto currencies = core::json::Parser::create<json::Currencies>(body, buffer);
      server::Trace event(trace_info, currencies);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::Currencies> &event) {
  auto &[trace_info, currencies] = event;
  log::info<4>("currencies={}"sv, currencies);
}

// symbols

void Rest::get_symbols() {
  profile_.symbols([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v1/symbols"sv;
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
        "symbols"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_symbols_ack(event, sequence);
        });
  });
}

void Rest::get_symbols_ack(const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.symbols_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = RestState::SYMBOLS;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto symbols = core::json::Parser::create<json::Symbols>(body, buffer);
      server::Trace event(trace_info, symbols);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void Rest::operator()(const server::Trace<json::Symbols> &event) {
  auto &[trace_info, symbols] = event;
  log::info<4>("symbols={}"sv, symbols);
  // reference data
  std::vector<std::string> symbols_2;
  // symbols_2.reserve(std::size(symbols.data));
  size_t counter = 0;
  for (size_t i = 0; i < std::size(symbols.data); ++i) {
    auto &item = symbols.data[i];
    log::info<2>("item={}"sv, item);
    auto &symbol = item.symbol;
    if (shared_.discard_symbol(item.symbol))
      continue;
    if (all_symbols_.emplace(symbol).second)  // only include new
      symbols_2.emplace_back(symbol);
    ++counter;
    const ReferenceData reference_data{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = symbol,
        .description = item.name,
        .security_type = {},
        .base_currency = item.base_currency,
        .quote_currency = item.quote_currency,
        .commission_currency = {},
        .tick_size = item.price_increment,  // XXX check
        .multiplier = 1.0,
        .min_trade_vol = item.quote_min_size,         // XXX check
        .max_trade_vol = item.quote_max_size,         // XXX check
        .trade_vol_step_size = item.quote_increment,  // XXX check
        .option_type = {},
        .strike_currency = {},
        .strike_price = NaN,
        .underlying = {},
        .time_zone = {},
        .issue_date = {},
        .settlement_date = {},
        .expiry_datetime = {},
        .expiry_datetime_utc = {},
    };
    server::create_trace_and_dispatch(handler_, trace_info, reference_data, true);
  }
  if (!std::empty(symbols_2)) {
    SymbolsUpdate symbols_update{
        .symbols = symbols_2,
    };
    handler_(symbols_update);
  }
  if (ROQ_UNLIKELY(counter > 0))
    log::info("Symbols {} / {}"sv, counter, std::size(symbols.data));
  // market status
  for (auto &item : symbols.data) {
    auto &symbol = item.symbol;
    if (all_symbols_.find(symbol) == std::end(all_symbols_))
      continue;
    const MarketStatus market_status{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = symbol,
        .trading_status = item.enable_trading ? TradingStatus::OPEN : TradingStatus::CLOSE,
    };
    server::create_trace_and_dispatch(handler_, trace_info, market_status, true);
  }
}

// order-book

void Rest::get_order_book(const std::string_view &symbol) {
  profile_.order_book([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v3/market/orderbook/level2"sv;
    auto query = fmt::format("?symbol={}"sv, symbol);
    auto headers = security_.create_signature_api_v2(method, path, query, {});
    core::web::Request request{
        .method = method,
        .path = path,
        .query = query,
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    connection_(
        "order_book"sv,
        request,
        [this, symbol = std::string{symbol}]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_order_book_ack(event, symbol);
        });
  });
}

void Rest::get_order_book_ack(
    const server::Trace<core::web::Response> &event, const std::string_view &symbol) {
  profile_.order_book_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto order_book = core::json::Parser::create<json::OrderBook>(body, buffer);
      server::Trace event(trace_info, order_book);
      (*this)(event, symbol);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      // ???
    }
  });
}

void Rest::operator()(server::Trace<json::OrderBook> const &event, const std::string_view &symbol) {
  // auto &[trace_info, order_book] = event;
  auto &trace_info = event.trace_info;
  auto &order_book = event.value;
  log::info<4>(R"(order_book={}, symbol="{}")"sv, order_book, symbol);
  auto &data = order_book.data;
  auto sequence = data.sequence;
  log::debug(R"(symbol="{}", sequence={})"sv, symbol, sequence);
  auto exchange_time_utc = data.time;
  auto &collector = shared_.mbp_collector[symbol];
  core::back_emplacer bids(shared_.bids), asks(shared_.asks);
  for (auto &item : data.bids) {
    if (utils::compare(item.price, 0.0) != 0) {
      bids.emplace_back([&item](auto &result) { emplace(result, item.price, item.size); });
    }
  }
  for (auto &item : data.asks) {
    if (utils::compare(item.price, 0.0) != 0) {
      asks.emplace_back([&item](auto &result) { emplace(result, item.price, item.size); });
    }
  }
  try {
    collector(
        bids,
        asks,
        sequence,
        [&](auto &bids, auto &asks, auto sequence) {  // snapshot
          log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
          MarketByPriceUpdate market_by_price_update{
              .stream_id = stream_id_,
              .exchange = Flags::exchange(),
              .symbol = symbol,
              .bids = bids,
              .asks = asks,
              .update_type = UpdateType::SNAPSHOT,
              .exchange_time_utc = exchange_time_utc,
              .exchange_sequence = collector.last_sequence(),
              .price_decimals = {},
              .quantity_decimals = {},
              .checksum = {},
          };
          server::Trace event(trace_info, market_by_price_update);
          shared_(event, true, [&](auto &market_by_price) {
            collector.apply(market_by_price, sequence, false);
          });
        },
        [&](auto retries) {  // request
          log::debug(R"(REQUEST symbol="{}" (retries={}))"sv, symbol, retries);
          if (retries > Flags::ws_mbp_request_max_retries()) {
            log::fatal("Unexpected"sv);
          }
          auto now = trace_info.source_receive_time;
          shared_.request_queue.emplace_back(now + Flags::ws_mbp_request_delay(), symbol);
        });
  } catch (BadState &) {
    log::warn(R"(RESUBSCRIBE symbol="{}")"sv, symbol);
    // XXX HANS publish stale
    collector.clear();
    auto now = trace_info.source_receive_time;
    shared_.request_queue.emplace_back(now + Flags::ws_mbp_request_delay(), symbol);
  }
}

// queue

void Rest::check_request_queue(std::chrono::nanoseconds now) {
  while (!std::empty(shared_.request_queue)) {
    auto &tmp = shared_.request_queue.front();
    if (now < tmp.first)
      break;
    if (shared_.can_request(now, [&]() {
          auto &symbol = tmp.second;
          log::debug(R"(Requesting order book snapshot symbol="{}")"sv, symbol);
          get_order_book(symbol);
          shared_.request_queue.pop_front();
        })) {
    } else {
      return;
    }
  }
}

}  // namespace okex
}  // namespace roq
