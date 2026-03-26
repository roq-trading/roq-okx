/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/static_data.hpp"

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

// === CONSTANTS ===

namespace {
auto const NAME = "sd"sv;

auto const SUPPORTS = Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

size_t const MAX_DECODE_BUFFER_DEPTH = 2;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.ws.public_uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = settings.misc.test_local_interface,
      .uris = {&uri, 1},
      .host = settings.ws.public_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = {},  // note!
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

StaticData::StaticData(Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared)
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
          .login = create_metrics(shared.settings, name_, "login"sv),
          .status = create_metrics(shared.settings, name_, "status"sv),
          .instruments = create_metrics(shared.settings, name_, "instruments"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      account_{account}, shared_{shared}, download_{{}, [this](auto state) { return download(state); }} {
}

void StaticData::operator()(Event<Start> const &) {
  (*connection_).start();
}

void StaticData::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void StaticData::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if ((*connection_).ready()) {
    check_subscribe_queue(now);
  }
}

void StaticData::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.subscribe, metrics::Type::PROFILE)
      .write(profile_.unsubscribe, metrics::Type::PROFILE)
      .write(profile_.login, metrics::Type::PROFILE)
      .write(profile_.status, metrics::Type::PROFILE)
      .write(profile_.instruments, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

// web::socket::Client::Handler

void StaticData::operator()(web::socket::Client::Connected const &) {
}

void StaticData::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  subscribe_queue_.clear();
}

void StaticData::operator()(web::socket::Client::Ready const &) {
  download_.begin();
}

void StaticData::operator()(web::socket::Client::Close const &) {
}

void StaticData::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void StaticData::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void StaticData::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected: binary"sv);
}

void StaticData::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
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

uint32_t StaticData::download(StaticDataState state) {
  switch (state) {
    using enum StaticDataState;
    case UNDEFINED:
      assert(false);
      break;
    case LOGIN:
      if (std::empty(account_)) {
        log::info("Using public channels (no authentication)"sv);
        return 0;
      } else {
        (*this)(ConnectionStatus::LOGIN_SENT);
        login();
        return 1;
      }
    case DONE:
      (*this)(ConnectionStatus::READY);
      subscribe_static();
      return 0;
  }
  assert(false);
  return 0;
}

void StaticData::login() {
  auto now = clock::get_realtime<std::chrono::seconds>();
  auto timestamp = fmt::format("{}"sv, now.count());
  auto sign = account_.create_sign(timestamp);
  auto message = fmt::format(
      R"({{)"
      R"("op":"login",)"
      R"("args":[{{)"
      R"("apiKey":"{}",)"
      R"("passphrase":"{}",)"
      R"("timestamp":"{}",)"
      R"("sign":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      account_.get_key(),
      account_.get_passphrase(),
      timestamp,
      sign);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void StaticData::subscribe_static() {
  subscribe("status"sv);
  subscribe("instruments"sv, "instType"sv, "SPOT"sv);
  subscribe("instruments"sv, "instType"sv, "SWAP"sv);
  subscribe("instruments"sv, "instType"sv, "FUTURES"sv);
  // subscribe("instruments"sv, "instType"sv, "OPTION"sv);
}

void StaticData::subscribe(std::string_view const &channel) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel);
  subscribe_queue_.emplace_back(message);
}

void StaticData::subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}",)"
      R"("{}":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel,
      selector,
      value);
  subscribe_queue_.emplace_back(message);
}

void StaticData::parse(std::string_view const &message) {
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

void StaticData::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
  });
}

void StaticData::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("subscribe={}"sv, subscribe);
    if (subscribe.arg.channel == json::Channel::INSTRUMENTS && subscribe.arg.inst_type == "FUTURES"sv) {
      log::info("Request instruments..."sv);
      shared_.instruments.request = clock::get_system();
    }
  });
}

void StaticData::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("unsubscribe={}"sv, unsubscribe);
  });
}

void StaticData::operator()(Trace<json::Status> const &event) {
  profile_.status([&]() {
    auto &[trace_info, status] = event;
    log::info("status={}"sv, status);
    for (auto &item : status.data) {
      if (item.state == json::State::ONGOING) {
        log::warn("*** DISCONNECT: ONGOING MAINTENANCE ***"sv);
        (*connection_).close();
        return;
      }
    }
  });
}

void StaticData::operator()(Trace<json::Instruments> const &event) {
  profile_.instruments([&]() {
    auto &[trace_info, instruments] = event;
    log::info<1>("instruments={}"sv, instruments);
    (*connection_).touch(trace_info.source_receive_time);
    std::vector<Symbol> symbols;
    symbols.reserve(std::size(instruments.data));
    size_t counter = {};
    for (auto &item : instruments.data) {
      log::info<2>("item={}"sv, item);
      auto symbol = item.inst_id;
      auto discard = shared_.discard_symbol(symbol);
      auto base_currency = [&]() {
        if (std::empty(item.base_ccy)) {
          switch (item.ct_type) {
            using enum json::ContractType::type_t;
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
            using enum json::ContractType::type_t;
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
      create_trace_and_dispatch(handler_, trace_info, reference_data, true);
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
      create_trace_and_dispatch(handler_, trace_info, market_status, true);
      // trying to reduce the number of symbols where we next extra subscriptions
      // but still avoid not reducing too much
      switch (item.inst_type) {
        using enum json::InstrumentType::type_t;
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
    log::info<1>("Instruments {} / {}"sv, counter, std::size(instruments.data));
    if (!std::empty(symbols)) {
      auto symbols_update = SymbolsUpdate{
          .symbols = symbols,
      };
      handler_(symbols_update);
    }
  });
}

void StaticData::operator()(Trace<json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::BboTbt> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::BooksL2Tbt> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::FundingRate> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::ChannelConnCount> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("login={}"sv, login);
    (*connection_).touch(trace_info.source_receive_time);
    auto state = StaticDataState::LOGIN;
    download_.check_relaxed(state);
  });
}

void StaticData::operator()(Trace<json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Order> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::AmendOrder> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::CancelOrder> const &) {
  log::fatal("Unexpected"sv);
}

void StaticData::operator()(Trace<json::Candle> const &) {
  log::fatal("Unexpected"sv);
}

// helpers

void StaticData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch([&](auto now) { return shared_.rate_limiter.can_request(now); }, [&](auto &message) { (*connection_).send_text(message); }, now);
}

}  // namespace okx
}  // namespace roq
