/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/gateway/rest.hpp"

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/okx/protocol/json/map.hpp"
#include "roq/okx/protocol/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace gateway {

// === CONSTANTS ===

namespace {
auto const NAME = "dc"sv;

auto const SUPPORTS = Mask<SupportType>{};

size_t const MAX_DECODE_BUFFER_DEPTH = 2;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.rest.uri;
  auto config = web::rest::Client::Config{
      // connection
      .interface = settings.misc.test_local_interface,
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // request
      .allow_pipelining = true,
      .request_timeout = settings.rest.request_timeout,
      // response
      .suspend_on_retry_after = {},
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = settings.rest.ping_path,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::rest::Client::create(handler, context, config);
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};
}  // namespace

// === IMPLEMENTATION ===

Rest::Rest(Handler &handler, io::Context &context, uint16_t stream_id, Shared &shared)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .instruments = create_metrics(shared.settings, name_, "instruments"sv),
          .instruments_ack = create_metrics(shared.settings, name_, "instruments_ack"sv),
          .candles = create_metrics(shared.settings, name_, "candles"sv),
          .candles_ack = create_metrics(shared.settings, name_, "candles_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      shared_{shared} {
}

void Rest::operator()(Event<Start> const &) {
  (*connection_).start();
}

void Rest::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void Rest::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if (ready()) {
    check_request_queue(now);
  }
}

void Rest::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.instruments, metrics::Type::PROFILE)
      .write(profile_.instruments_ack, metrics::Type::PROFILE)
      .write(profile_.candles, metrics::Type::PROFILE)
      .write(profile_.candles_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY);
}

void Rest::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
  connection_status_ = connection_status;
  TraceInfo trace_info;
  auto stream_status = StreamStatus{
      .stream_id = stream_id_,
      .account = {},
      .supports = SUPPORTS,
      .transport = Transport::TCP,
      .protocol = Protocol::HTTP,
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

// web::rest::Client::Handler

void Rest::operator()(Trace<web::rest::Client::Connected> const &) {
  (*this)(ConnectionStatus::READY);
}

void Rest::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_instruments_ = {};
}

void Rest::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(shared_.dispatcher, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

// instruments

void Rest::get_instruments(std::string_view const &type) {
  profile_.instruments([&]() {
    auto query = fmt::format("?instType={}"sv, type);
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.market_data.instruments,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, type = std::string{type}]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_instruments_ack(event, type);
    };
    (*connection_)("instruments"sv, request, callback);
  });
}

void Rest::get_instruments_ack(Trace<web::rest::Response> const &event, std::string_view const &type) {
  profile_.instruments_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&](auto &body) {
      protocol::json::InstrumentsAck instruments_ack{body, decode_buffer_};
      if (instruments_ack.code == 0) {
        Trace event_2{event, instruments_ack};
        (*this)(event_2);
      } else {
        handle_error(Origin::EXCHANGE, RequestStatus::REJECTED, protocol::json::guess_error(instruments_ack.code), instruments_ack.msg);
      }
      if (type == "SPOT"sv) {
        download_instruments_.spot = false;
      } else if (type == "SWAP"sv) {
        download_instruments_.swap = false;
      } else if (type == "FUTURES"sv) {
        download_instruments_.futures = false;
        shared_.instruments.response = clock::get_system();
      } else if (type == "OPTION"sv) {
        // download_instruments_.option = false;
        // shared_.instruments.response = clock::get_system();
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void Rest::operator()(Trace<protocol::json::InstrumentsAck> const &event) {
  auto &[trace_info, instruments_ack] = event;
  log::info<4>("instruments_ack={}"sv, instruments_ack);
  std::vector<Symbol> symbols;
  symbols.reserve(std::size(instruments_ack.data));
  size_t counter = {};
  for (auto &item : instruments_ack.data) {
    log::info<2>("item={}"sv, item);
    auto symbol = item.inst_id;
    auto discard = shared_.dispatcher.discard_symbol(symbol);
    auto base_currency = [&]() {
      if (std::empty(item.base_ccy)) {
        switch (item.ct_type) {
          using enum protocol::json::ContractType::type_t;
          case UNDEFINED_INTERNAL:
          case UNKNOWN_INTERNAL:
            break;
          case LINEAR:
            return item.ct_val_ccy;
          case INVERSE:
            return item.settle_ccy;
        }
      }
      return item.base_ccy;
    }();
    auto quote_currency = [&]() {
      if (std::empty(item.quote_ccy)) {
        switch (item.ct_type) {
          using enum protocol::json::ContractType::type_t;
          case UNDEFINED_INTERNAL:
          case UNKNOWN_INTERNAL:
            break;
          case LINEAR:
            return item.settle_ccy;
          case INVERSE:
            return item.ct_val_ccy;
        }
      }
      return item.quote_ccy;
    }();
    auto reference_data = ReferenceData{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = symbol,
        .description = {},
        .security_type = map(item.inst_type),
        .external_security_id = utils::safe_cast(item.inst_id_code),
        .cfi_code = {},
        .base_currency = base_currency,
        .quote_currency = quote_currency,
        .settlement_currency = item.settle_ccy,
        .margin_currency = {},
        .commission_currency = {},
        .tick_size = item.tick_sz,
        .tick_size_steps = {},
        .multiplier = item.ct_mult,
        .min_notional = NaN,
        .min_trade_vol = item.min_sz,
        .max_trade_vol = NaN,
        .trade_vol_step_size = item.lot_sz,
        .option_type = map(item.opt_type),
        .strike_currency = {},
        .strike_price = item.stk,
        .underlying = item.uly,
        .time_zone = {},
        .issue_date = utils::safe_cast{item.list_time},
        .settlement_date = {},
        .expiry_datetime = utils::safe_cast{item.exp_time},
        .expiry_datetime_utc = utils::safe_cast{item.exp_time},
        .exchange_time_utc = {},
        .exchange_sequence = {},
        .sending_time_utc = {},
        .discard = discard,
    };
    create_trace_and_dispatch(shared_.dispatcher, trace_info, reference_data, true);
    if (discard) {
      log::info<1>(R"(Drop symbol="{}")"sv, symbol);
      continue;
    }
    if (shared_.all_symbols.emplace(symbol).second) {  // only include new
      symbols.emplace_back(symbol);
    }
    ++counter;
    auto market_status = MarketStatus{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = item.inst_id,
        .trading_status = map(item.state),
        .exchange_time_utc = {},
        .exchange_sequence = {},
        .sending_time_utc = {},
    };
    create_trace_and_dispatch(shared_.dispatcher, trace_info, market_status, true);
    // trying to reduce the number of symbols where we next extra subscriptions
    // but still avoid not reducing too much
    switch (item.inst_type) {
      using enum protocol::json::InstrumentType::type_t;
      case UNDEFINED_INTERNAL:
      case UNKNOWN_INTERNAL:
      case SPOT:
      case MARGIN:
        break;
      case SWAP:
      case FUTURES:
        if (shared_.extended_symbols.emplace(symbol).second) {
          log::info<2>(R"(DEBUG: symbol="{}")"sv, symbol);
        }
        break;
      case OPTION:
        break;
    }
  }
  log::info("Instruments {} / {}"sv, counter, std::size(instruments_ack.data));
  if (!std::empty(symbols)) {
    auto symbols_update = SymbolsUpdate{
        .symbols = symbols,
    };
    handler_(symbols_update);
  }
}

// candles

void Rest::get_candles(std::string_view const &symbol) {
  profile_.candles([&]() {
    auto query = fmt::format("?instId={}&bar=1m&limit={}"sv, symbol, shared_.settings.download.time_series_limit);
    auto request = web::rest::Request{
        .method = web::http::Method::GET,
        .path = shared_.api.market_data.candles,
        .query = query,
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = {},
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this, symbol = std::string{symbol}]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_candles_ack(event, symbol);
    };
    (*connection_)("candles"sv, request, callback);
  });
}

void Rest::get_candles_ack(Trace<web::rest::Response> const &event, std::string_view const &symbol) {
  profile_.candles_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&](auto &body) {
      protocol::json::CandlesAck candles_ack{body, decode_buffer_};
      if (candles_ack.code == 0) {
        Trace event_2{event, candles_ack};
        (*this)(event_2, symbol);
      } else {
        handle_error(Origin::EXCHANGE, RequestStatus::REJECTED, protocol::json::guess_error(candles_ack.code), candles_ack.msg);
      }
    };
    process_response(event, handle_error, handle_success);
  });
}

void Rest::operator()(Trace<protocol::json::CandlesAck> const &event, std::string_view const &symbol) {
  auto &[trace_info, candles_ack] = event;
  log::info<4>("candles_ack={}"sv, candles_ack);
  auto &bars = shared_.bars;
  bars.clear();
  for (auto &item : candles_ack.data) {
    auto confirmed = item.confirm != 0 ? true : false;
    if (confirmed && !shared_.settings.time_series.realtime) {
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
        .symbol = symbol,
        .data_source = DataSource::TRADE_SUMMARY,
        .interval = shared_.settings.time_series.interval,
        .origin = Origin::EXCHANGE,
        .bars = bars,
        .update_type = UpdateType::SNAPSHOT,
        .exchange_time_utc = {},  // XXX FIXME
    };
    create_trace_and_dispatch(shared_.dispatcher, trace_info, time_series_update, true);
  }
}

// request

void Rest::check_request_queue([[maybe_unused]] std::chrono::nanoseconds now) {
  if (!downloading() && shared_.instruments.response < shared_.instruments.request) {
    log::info("Download instruments..."sv);
    get_instruments("SPOT"sv);
    get_instruments("SWAP"sv);
    get_instruments("FUTURES"sv);
    download_instruments_.spot = download_instruments_.swap = download_instruments_.futures = true;
  }
  auto can_request = [&](auto now) { return shared_.rate_limiter.can_request(now); };
  auto request = [&](auto &symbol) { get_candles(symbol); };
  shared_.time_series_request_queue.dispatch(can_request, request, now);
}

// helpers

void Rest::process_response(web::rest::Response const &response, auto error_handler, auto success_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case UNKNOWN:
      case INFORMATIONAL_RESPONSE:
        response.expect(web::http::Status::OK);  // throws
        break;
      case SUCCESS:
        success_handler(body);
        break;
      case REDIRECTION:
        log::fatal("Unexpected: URL is being redirected"sv);
      case CLIENT_ERROR: {
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, text);
        break;
      }
      case SERVER_ERROR: {
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, text);
        break;
      }
    }
  } catch (NetworkError &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::GATEWAY, e.request_status(), e.error(), e.what());
  } catch (std::exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, e.what());
  }
}

}  // namespace gateway
}  // namespace okx
}  // namespace roq
