/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/gateway/business.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/okx/json/map.hpp"
#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace gateway {

// === CONSTANTS ===

namespace {
auto const NAME = "md"sv;

auto const SUPPORTS = Mask{
    SupportType::TIME_SERIES,
};

size_t const MAX_DECODE_BUFFER_DEPTH = 2;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.ws.business_uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = settings.misc.test_local_interface,
      .uris = {&uri, 1},
      .host = settings.ws.business_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = settings.net.disconnect_on_idle_timeout,
      .always_reconnect = true,
      // proxy
      .proxy = {},
      // http
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = {},
      .ping_frequency = settings.ws.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};
}  // namespace

// === IMPLEMENTATION ===

Business::Business(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .error = create_metrics(shared.settings, name_, "error"sv),
          .subscribe = create_metrics(shared.settings, name_, "subscribe"sv),
          .unsubscribe = create_metrics(shared.settings, name_, "unsubscribe"sv),
          .candles = create_metrics(shared.settings, name_, "candles"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      shared_{shared} {
}

void Business::operator()(Event<Start> const &) {
  (*connection_).start();
}

void Business::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void Business::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if ((*connection_).ready()) {
    check_subscribe_queue(now);
  }
}

void Business::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.subscribe, metrics::Type::PROFILE)
      .write(profile_.unsubscribe, metrics::Type::PROFILE)
      .write(profile_.candles, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

void Business::subscribe(size_t start_from) {
  if (ready()) {
    subscribe(shared_.symbols.get_all(start_from));
  }
}

// web::socket::Client::Handler

void Business::operator()(web::socket::Client::Connected const &) {
}

void Business::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  subscribe_queue_.clear();
}

void Business::operator()(web::socket::Client::Ready const &) {
  (*this)(ConnectionStatus::DOWNLOADING, "subscribe"sv);
  subscribe_static();
  subscribe(shared_.symbols.get_all());
  (*this)(ConnectionStatus::READY);
}

void Business::operator()(web::socket::Client::Close const &) {
}

void Business::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Business::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void Business::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected: binary"sv);
}

void Business::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
  connection_status_ = connection_status;
  TraceInfo trace_info;
  auto stream_status = StreamStatus{
      .stream_id = stream_id_,
      .account = {},
      .supports = SUPPORTS,
      .transport = Transport::TCP,
      .protocol = Protocol::WS,
      .encoding = {Encoding::JSON},
      .priority = Priority::PRIMARY,
      .connection_status = connection_status_,
      .reason = reason,
      .interface = (*connection_).get_interface(),
      .authority = (*connection_).get_current_authority(),
      .path = (*connection_).get_current_path(),
      .proxy = (*connection_).get_proxy(),
  };
  log::info("stream_status={}"sv, stream_status);
  create_trace_and_dispatch(handler_, trace_info, stream_status);
}

void Business::subscribe_static() {
  // subscribe("status"sv);
}

void Business::subscribe(std::span<Symbol const> const &symbols) {
  if (std::empty(symbols)) {
    return;
  }
  if (shared_.settings.download.time_series_lookback.count()) {
    subscribe("candle1m"sv, "instId"sv, symbols);
    for (auto &symbol : symbols) {
      shared_.time_series_request_queue.emplace_back(symbol);
    }
  }
}

void Business::subscribe(std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values) {
  assert(!std::empty(values));
  auto prefix = fmt::format(
      R"({{)"
      R"("channel":"{}",)"
      R"("{}":")"sv,
      channel,
      selector);
  auto separator = fmt::format(R"("}},{})"sv, prefix);
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[)"
      R"({}{}"}})"
      R"(])"
      R"(}})"sv,
      prefix,
      fmt::join(values, separator));
  subscribe_queue_.emplace_back(message);
}

void Business::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(*** PLEASE REPORT *** message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!json::Parser::dispatch(*this, message, decode_buffer_, trace_info, shared_.settings.experimental.allow_unknown_event_types)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

// json::Parser::Handler

void Business::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
  });
}

void Business::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("subscribe={}"sv, subscribe);
    if (subscribe.arg.channel == json::Channel::INSTRUMENTS && subscribe.arg.inst_type == "FUTURES"sv) {
      log::info("Request instruments..."sv);
      shared_.instruments.request = clock::get_system();
    }
  });
}

void Business::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("unsubscribe={}"sv, unsubscribe);
  });
}

void Business::operator()(Trace<json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::BboTbt> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::BooksL2Tbt> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::FundingRate> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::ChannelConnCount> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Login> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Order> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::AmendOrder> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::CancelOrder> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<json::Candle> const &event) {
  auto &[trace_info, candle] = event;
  log::info<3>("candle={}"sv, candle);
  (*connection_).touch(trace_info.source_receive_time);
  auto &bars = shared_.bars;
  bars.clear();
  for (auto &item : candle.data) {
    auto confirmed = item.confirm != 0;
    if (!confirmed && !shared_.settings.time_series.realtime) {
      continue;
    }
    auto bar = Bar{
        .begin_time_utc = utils::safe_cast(item.timestamp),
        .confirmed = confirmed,
        .open_price = item.open,
        .high_price = item.highest,
        .low_price = item.lowest,
        .close_price = item.close,
        .quantity = item.volume,
        .base_amount = NaN,
        .quote_amount = item.volume_ccy_quote,
        .number_of_trades = {},
        .vwap = NaN,
    };
    bars.emplace_back(std::move(bar));
  }
  if (!std::empty(bars)) {
    auto time_series_update = TimeSeriesUpdate{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = candle.arg.inst_id,
        .data_source = DataSource::TRADE_SUMMARY,
        .interval = shared_.settings.time_series.interval,
        .origin = Origin::EXCHANGE,
        .bars = bars,
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = {},  // XXX FIXME
    };
    create_trace_and_dispatch(handler_, trace_info, time_series_update, true);
  }
}

// request

void Business::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch([&](auto now) { return shared_.rate_limiter.can_request(now); }, [&](auto &message) { (*connection_).send_text(message); }, now);
}

}  // namespace gateway
}  // namespace okx
}  // namespace roq
