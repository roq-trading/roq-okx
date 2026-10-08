/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/gateway/business.hpp"

#include "roq/logging.hpp"

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/okx/protocol/json/map.hpp"
#include "roq/okx/protocol/json/utils.hpp"

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

auto create_connection(auto &handler, auto &settings, auto &context, auto &shared) {
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
  return web::socket::Client::create(handler, context, config, shared.throttle, []() { return std::string(); });
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};
}  // namespace

// === IMPLEMENTATION ===

Business::Business(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, connection_{create_connection(*this, shared.settings, context, shared)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .error = create_metrics(shared.settings, name_, "error"sv),
          .subscribe = create_metrics(shared.settings, name_, "subscribe"sv),
          .unsubscribe = create_metrics(shared.settings, name_, "unsubscribe"sv),
          .notice = create_metrics(shared.settings, name_, "notice"sv),
          .candles = create_metrics(shared.settings, name_, "candles"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      shared_{shared} {
}

// server::Stream

void Business::operator()(Event<Start> const &) {
  (*connection_).start();
}

void Business::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void Business::operator()(Event<Timer> const &event) {
  auto &[trace_info, timer] = event;
  (*connection_).refresh(timer.now);
  if ((*connection_).ready()) {
    check_subscribe_queue(timer.now);
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
      .write(profile_.notice, metrics::Type::PROFILE)
      .write(profile_.candles, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

void Business::operator()(Trace<ConnectionStatus> const &event, std::string_view const &reason) {
  auto &[trace_info, connection_status] = event;
  connection_status_ = connection_status;
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
  create_trace_and_dispatch(shared_.dispatcher, trace_info, stream_status);
}

// server::MarketDataStream

void Business::subscribe(size_t start_from) {
  if (ready()) {
    subscribe(shared_.symbols.get_all(start_from));
  }
}

// web::socket::Client::Handler

void Business::operator()(Trace<web::socket::Connected> const &) {
}

void Business::operator()(Trace<web::socket::Disconnected> const &event) {
  auto &[trace_info, disconnected] = event;
  ++counter_.disconnect;
  create_trace_and_dispatch_2(trace_info, ConnectionStatus::DISCONNECTED);
  subscribe_queue_.clear();
}

void Business::operator()(Trace<web::socket::Ready> const &event) {
  auto &[trace_info, ready] = event;
  create_trace_and_dispatch_2(trace_info, ConnectionStatus::DOWNLOADING, "subscribe"sv);
  subscribe_static();
  subscribe(shared_.symbols.get_all());
  create_trace_and_dispatch_2(trace_info, ConnectionStatus::READY);
}

void Business::operator()(Trace<web::socket::Close> const &event) {
  auto &[trace_info, close] = event;
  log::warn("close={}"sv, close);
}

void Business::operator()(Trace<web::socket::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(shared_.dispatcher, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Business::operator()(Trace<web::socket::Text> const &event) {
  auto &[trace_info, text] = event;
  parse(text.payload);
}

void Business::operator()(Trace<web::socket::Binary> const &) {
  log::fatal("Unexpected: binary"sv);
}

// protocol::json::Parser::Handler

void Business::operator()(Trace<protocol::json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
  });
}

void Business::operator()(Trace<protocol::json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("subscribe={}"sv, subscribe);
    if (subscribe.arg.channel == protocol::json::Channel::INSTRUMENTS && subscribe.arg.inst_type == "FUTURES"sv) {
      log::info("Request instruments..."sv);
      shared_.instruments.request = clock::get_system();
    }
  });
}

void Business::operator()(Trace<protocol::json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("unsubscribe={}"sv, unsubscribe);
  });
}

void Business::operator()(Trace<protocol::json::Notice> const &event) {
  profile_.notice([&]() {
    auto &[trace_info, notice] = event;
    log::warn("notice={}"sv, notice);
  });
}

void Business::operator()(Trace<protocol::json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::BboTbt> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::BooksL2Tbt> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::FundingRate> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::ChannelConnCount> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Login> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Order> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::AmendOrder> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::CancelOrder> const &) {
  log::fatal("Unexpected"sv);
}

void Business::operator()(Trace<protocol::json::Candle> const &event) {
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
    create_trace_and_dispatch(shared_.dispatcher, trace_info, time_series_update, true);
  }
}

// helpers

void Business::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch([&](auto now) { return shared_.rate_limiter.can_request(now); }, [&](auto &message) { (*connection_).send_text(message); }, now);
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
      if (!protocol::json::Parser::dispatch(*this, message, decode_buffer_, trace_info, shared_.settings.experimental.allow_unknown_event_types)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

}  // namespace gateway
}  // namespace okx
}  // namespace roq
