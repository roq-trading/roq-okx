/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/market_data.hpp"

#include <algorithm>

#include "roq/mask.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/back_emplacer.hpp"
#include "roq/core/charconv.hpp"

#include "roq/core/tools/exception.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/socket/client_factory.hpp"

#include "roq/okx/flags.hpp"

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "md"sv;

Mask const SUPPORTS{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::ws_public_uri();
  web::socket::Client::Config config{
      .always_reconnect = true,
      .connection_timeout = server::Flags::net_connection_timeout(),
      .disconnect_on_idle_timeout = server::Flags::net_disconnect_on_idle_timeout(),
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .query = {},
      .ping_frequency = Flags::ws_ping_freq(),
      .read_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

MarketData::MarketData(
    Handler &handler, io::Context &context, uint32_t stream_id, Security &security, Shared &shared, size_t index)
    : handler_(handler), stream_id_(stream_id), name_(create_name(stream_id_)), index_(index),
      connection_(create_connection(*this, context)), decode_buffer_(Flags::decode_buffer_size()),
      request_id_(static_cast<uint64_t>(stream_id_) * 1000000),  // scale (debugging)
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .error = create_metrics(name_, "error"sv),
          .subscribe = create_metrics(name_, "subscribe"sv),
          .unsubscribe = create_metrics(name_, "unsubscribe"sv),
          .login = create_metrics(name_, "login"sv),
          .status = create_metrics(name_, "status"sv),
          .instruments = create_metrics(name_, "instruments"sv),
          .estimated_price = create_metrics(name_, "estimated_price"sv),
          .price_limit = create_metrics(name_, "price_limit"sv),
          .mark_price = create_metrics(name_, "mark_price"sv),
          .tickers = create_metrics(name_, "tickers"sv),
          .trades = create_metrics(name_, "trades"sv),
          .bbo_tbt = create_metrics(name_, "bbo_tbt"sv),
          .books_l2_tbt = create_metrics(name_, "books_l2_tbt"sv),
          .index_tickers = create_metrics(name_, "index_tickers"sv),
          .funding_rate = create_metrics(name_, "funding_rate"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      security_(security), shared_(shared), download_({}, [this](auto state) { return download(state); }) {
}

void MarketData::operator()(Event<Start> const &) {
  (*connection_).start();
}

void MarketData::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void MarketData::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if ((*connection_).ready())
    check_subscribe_queue(now);
}

void MarketData::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.subscribe, metrics::PROFILE)
      .write(profile_.unsubscribe, metrics::PROFILE)
      .write(profile_.login, metrics::PROFILE)
      .write(profile_.status, metrics::PROFILE)
      .write(profile_.instruments, metrics::PROFILE)
      .write(profile_.estimated_price, metrics::PROFILE)
      .write(profile_.price_limit, metrics::PROFILE)
      .write(profile_.mark_price, metrics::PROFILE)
      .write(profile_.tickers, metrics::PROFILE)
      .write(profile_.trades, metrics::PROFILE)
      .write(profile_.bbo_tbt, metrics::PROFILE)
      .write(profile_.books_l2_tbt, metrics::PROFILE)
      .write(profile_.index_tickers, metrics::PROFILE)
      .write(profile_.funding_rate, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketData::subscribe(size_t start_from) {
  if (ready())
    subscribe(shared_.symbols.get_slice(index_, start_from));
}

void MarketData::operator()(web::socket::Client::Connected const &) {
}

void MarketData::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  subscribe_queue_.clear();
}

void MarketData::operator()(web::socket::Client::Ready const &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void MarketData::operator()(web::socket::Client::Close const &) {
}

void MarketData::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  const ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void MarketData::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected: binary"sv);
}

void MarketData::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    const StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t MarketData::download(MarketDataState state) {
  switch (state) {
    using enum MarketDataState;
    case UNDEFINED:
      assert(false);
      break;
    case LOGIN:
      if (std::empty(security_)) {
        log::info("Using public channels (no authentication)"sv);
        return 0;
      } else {
        login();
        return 1;
      }
    case DONE:
      (*this)(ConnectionStatus::READY);
      subscribe_static();
      subscribe();  // note! must be *after* READY
      return {};
  }
  assert(false);
  return {};
}

void MarketData::login() {
  auto now = clock::get_realtime<std::chrono::seconds>();
  auto timestamp = fmt::format("{}"sv, now.count());
  auto sign = security_.create_sign(timestamp);
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
      security_.get_key(),
      security_.get_passphrase(),
      timestamp,
      sign);
  log::debug("message={}"sv, message);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void MarketData::subscribe_static() {
  if (index_ != 0)
    return;
  subscribe("status"sv);
  subscribe("instruments"sv, "instType"sv, "SPOT"sv);
  subscribe("instruments"sv, "instType"sv, "SWAP"sv);
  subscribe("instruments"sv, "instType"sv, "FUTURES"sv);
  subscribe("instruments"sv, "instType"sv, "OPTION"sv);
  // subscribe("estimated-price"sv, "instType"sv, "FUTURES"sv);
  // subscribe("estimated-price"sv, "instType"sv, "OPTION"sv);
}

void MarketData::subscribe(std::span<Symbol const> const &symbols) {
  if (std::empty(symbols))
    return;
  // subscribe("price-limit"sv, "instType"sv, symbols);
  // subscribe("mark-price"sv, "instType"sv, symbols);
  subscribe("tickers"sv, "instId"sv, symbols);
  subscribe("trades"sv, "instId"sv, symbols);
  subscribe("bbo-tbt"sv, "instId"sv, symbols);
  auto get_books_channel = [](auto security) {
    std::string_view result;
    auto vip = security && !flags::Flags::ws_books_use_public();
    switch (flags::Flags::ws_books_depth()) {
      case 5:
        result = "books5"sv;
        break;
      case 50:
        result = "books50-l2-tbt"sv;
        if (!vip)
          log::fatal(R"(Channel "{}" requires authentication (only available to VIP members))"sv, result);
        break;
      case 400:
        result = vip ? "books-l2-tbt"sv : "books"sv;
        break;
      default:
        log::fatal("Unsupported --ws_books_depth={}"sv, flags::Flags::ws_books_depth());
    }
    log::info(R"(DEBUG: using channel="{}")"sv, result);
    return result;
  };
  subscribe(get_books_channel(!std::empty(security_)), "instId"sv, symbols);
  for (auto &symbol : symbols) {
    if (flags::Flags::include_bad_subscriptions() ||
        shared_.extended_symbols.find(symbol) != shared_.extended_symbols.end()) {
      subscribe("index-tickers"sv, "instId"sv, symbol);
      subscribe("funding-rate"sv, "instId"sv, symbol);
    }
  }
}

void MarketData::subscribe(std::string_view const &channel) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel);
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(message);
}

void MarketData::subscribe(
    std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
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
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(message);
}

void MarketData::subscribe(
    std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values) {
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
  log::debug("message={}"sv, message);
  subscribe_queue_.emplace_back(message);
}

void MarketData::parse(std::string_view const &message) {
  profile_.parse([&]() {
    try {
      // log::debug(R"(message="{}")"sv, message);
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
      if (json::Parser::dispatch(*this, message, buffer, trace_info)) {
      } else {
        log::fatal(R"(message="{}")"sv, message);
      }
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void MarketData::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("event={{error={}, trace_info={}}}"sv, error, trace_info);
  });
}

void MarketData::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{subscribe={}, trace_info={}}}"sv, subscribe, trace_info);
    log::debug("event={{subscribe={}, trace_info={}}}"sv, subscribe, trace_info);
  });
}

void MarketData::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
    log::debug("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
  });
}

void MarketData::operator()(Trace<json::Status> const &event) {
  profile_.status([&]() {
    auto &[trace_info, status] = event;
    log::info("event={{status={}, trace_info={}}}"sv, status, trace_info);
    if (status.state == json::State::ONGOING) {
      log::warn("*** DISCONNECT: ONGOING MAINTENANCE ***"sv);
      (*connection_).close();
    }
  });
}

void MarketData::operator()(Trace<json::Instruments> const &event) {
  profile_.instruments([&]() {
    auto &[trace_info, instruments] = event;
    log::info<1>("event={{instruments={}, trace_info={}}}"sv, instruments, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    std::vector<Symbol> symbols;
    symbols.reserve(std::size(instruments.data));
    size_t counter = {};
    for (auto &item : instruments.data) {
      log::info<2>("item={}"sv, item);
      auto symbol = item.inst_id;
      auto discard = shared_.discard_symbol(symbol);
      auto security_type = json::map(item.inst_type);
      auto option_type = json::map(item.opt_type);
      const ReferenceData reference_data{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = symbol,
          .description = {},
          .security_type = security_type,
          .base_currency = item.base_ccy,
          .quote_currency = item.quote_ccy,
          .margin_currency = item.settle_ccy,
          .commission_currency = {},
          .tick_size = item.tick_sz,
          .multiplier = item.ct_mult,
          .min_notional = NaN,
          .min_trade_vol = item.min_sz,
          .max_trade_vol = NaN,
          .trade_vol_step_size = NaN,
          .option_type = option_type,
          .strike_currency = {},
          .strike_price = item.stk,
          .underlying = item.uly,
          .time_zone = {},
          .issue_date = utils::safe_cast(item.list_time),
          .settlement_date = {},
          .expiry_datetime = utils::safe_cast(item.exp_time),
          .expiry_datetime_utc = utils::safe_cast(item.exp_time),
          .discard = discard,
      };
      create_trace_and_dispatch(handler_, trace_info, reference_data, true);
      if (discard)
        continue;
      if (all_symbols_.emplace(symbol).second)  // only include new
        symbols.emplace_back(symbol);
      ++counter;
      auto trading_status = json::map(item.state);
      const MarketStatus market_status{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .trading_status = trading_status,
      };
      create_trace_and_dispatch(handler_, trace_info, market_status, true);
      // trying to reduce the number of symbols where we next extra subscriptions
      // but still avoid not reducing too much
      switch (item.inst_type) {
        using enum json::InstrumentType::type_t;
        case UNDEFINED:
        case UNKNOWN:
        case SPOT:
        case MARGIN:
          break;
        case SWAP:
        case FUTURES:
          if (shared_.extended_symbols.emplace(symbol).second)
            log::info<2>(R"(DEBUG: symbol="{}")"sv, symbol);
          break;
        case OPTION:
          break;
      }
    }
    log::info("Instruments {} / {}"sv, counter, std::size(instruments.data));
    if (!std::empty(symbols)) {
      SymbolsUpdate symbols_update{
          .symbols = symbols,
      };
      handler_(symbols_update);
    }
  });
}

void MarketData::operator()(Trace<json::EstimatedPrice> const &event) {
  profile_.estimated_price([&]() {
    auto &[trace_info, estimated_price] = event;
    log::info<3>("event={{estimated_price={}, trace_info={}}}"sv, estimated_price, trace_info);
    log::debug("event={{estimated_price={}, trace_info={}}}"sv, estimated_price, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(Trace<json::PriceLimit> const &event) {
  profile_.price_limit([&]() {
    auto &[trace_info, price_limit] = event;
    log::info<3>("event={{price_limit={}, trace_info={}}}"sv, price_limit, trace_info);
    log::debug("event={{price_limit={}, trace_info={}}}"sv, price_limit, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(Trace<json::MarkPrice> const &event) {
  profile_.mark_price([&]() {
    auto &[trace_info, mark_price] = event;
    log::info<3>("event={{mark_price={}, trace_info={}}}"sv, mark_price, trace_info);
    log::debug("event={{mark_price={}, trace_info={}}}"sv, mark_price, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(Trace<json::Tickers> const &event) {
  profile_.tickers([&]() {
    auto &[trace_info, tickers] = event;
    log::info<3>("event={{tickers={}, trace_info={}}}"sv, tickers, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : tickers.data) {
      Statistics statistics[] = {
          {
              .type = StatisticsType::OPEN_PRICE,
              .value = item.open24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::HIGHEST_TRADED_PRICE,
              .value = item.high24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::LOWEST_TRADED_PRICE,
              .value = item.low24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::TRADE_VOLUME,
              .value = item.vol24h,  // note! not sure...
              .begin_time_utc = {},
              .end_time_utc = {},
          },
      };
      const StatisticsUpdate statistics_update{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = utils::safe_cast(item.ts),
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::Trades> const &event) {
  profile_.trades([&]() {
    auto &[trace_info, trades] = event;
    log::info<3>("event={{trades={}, trace_info={}}}"sv, trades, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    auto create_trade = []<typename T>(T &result, auto const &value) {
      new (&result) T{
          .side = json::map(value.side),
          .price = value.px,
          .quantity = value.sz,
          .trade_id = value.trade_id,
          .taker_order_id = {},
          .maker_order_id = {},
      };
    };
    core::back_emplacer trades_{shared_.trades};
    std::string_view symbol;
    std::chrono::nanoseconds exchange_time_utc = {};
    for (auto &item : trades.data) {
      if (utils::update(symbol, item.inst_id) && !std::empty(trades_)) {
        assert(!std::empty(symbol));
        const TradeSummary trade_summary{
            .stream_id = stream_id_,
            .exchange = Flags::exchange(),
            .symbol = symbol,
            .trades = trades_,
            .exchange_time_utc = exchange_time_utc,
            .exchange_sequence = {},
        };
        create_trace_and_dispatch(handler_, trace_info, trade_summary, false);
        trades_.clear();
        exchange_time_utc = {};
      }
      utils::update_max(exchange_time_utc, item.ts);
      trades_.emplace_back([&](auto &result) { create_trade(result, item); });
    }
    if (!std::empty(trades_)) {
      assert(!std::empty(symbol));
      const TradeSummary trade_summary{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = symbol,
          .trades = trades_,
          .exchange_time_utc = exchange_time_utc,
          .exchange_sequence = {},
      };
      create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
    }
  });
}

void MarketData::operator()(Trace<json::BboTbt> const &event, std::string_view const &inst_id) {
  profile_.bbo_tbt([&]() {
    auto &[trace_info, bbo_tbt] = event;
    log::info<3>("event={{bbo_tbt={}, trace_info={}}}"sv, bbo_tbt, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    auto &bids = bbo_tbt.bids;
    auto &asks = bbo_tbt.asks;
    if (std::size(bids) > 1 || std::size(asks) > 1)
      log::fatal("Unexpected"sv);
    const TopOfBook top_of_book{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = inst_id,
        .layer{
            .bid_price = std::empty(bids) ? NaN : bids[0].price,
            .bid_quantity = std::empty(bids) ? NaN : bids[0].size,
            .ask_price = std::empty(asks) ? NaN : asks[0].price,
            .ask_quantity = std::empty(asks) ? NaN : asks[0].size,
        },
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(bbo_tbt.ts),
        .exchange_sequence = {},
    };
    create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
  });
}

void MarketData::operator()(
    Trace<json::BooksL2Tbt> const &event, std::string_view const &inst_id, json::Action action) {
  profile_.books_l2_tbt([&]() {
    auto &[trace_info, books_l2_tbt] = event;
    log::info<3>("event={{books_l2_tbt={}, action={}, trace_info={}}}"sv, books_l2_tbt, action, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    auto snapshot = action == json::Action::SNAPSHOT;
    auto create_mbp_update = []<typename T>(T &result, auto const &item) {
      new (&result) T{
          .price = item.price,
          .quantity = item.size,
          .implied_quantity = NaN,
          .number_of_orders = utils::safe_cast(item.orders),
          .update_action = {},
          .price_level = {},
      };
    };
    core::back_emplacer bids{shared_.bids}, asks{shared_.asks};
    for (auto &item : books_l2_tbt.bids)
      bids.emplace_back([&](auto &result) { create_mbp_update(result, item); });
    for (auto &item : books_l2_tbt.asks)
      asks.emplace_back([&](auto &result) { create_mbp_update(result, item); });
    // XXX HANS validate checksum
    auto update_type = snapshot ? UpdateType::SNAPSHOT : UpdateType::INCREMENTAL;
    const MarketByPriceUpdate market_by_price_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = inst_id,
        .bids = bids,
        .asks = asks,
        .update_type = update_type,
        .exchange_time_utc = utils::safe_cast(books_l2_tbt.ts),
        .exchange_sequence = {},
        .price_decimals = {},
        .quantity_decimals = {},
        .checksum = {},
    };
    log::info<3>("market_by_price_update={}"sv, market_by_price_update);
    try {
      create_trace_and_dispatch(handler_, trace_info, market_by_price_update, true, false);
    } catch (BadState &) {
      // resubscribe_order_book_l2(symbol);
    }
  });
}

void MarketData::operator()(Trace<json::IndexTickers> const &event) {
  profile_.index_tickers([&]() {
    auto &[trace_info, index_tickers] = event;
    log::info<3>("event={{index_tickers={}, trace_info={}}}"sv, index_tickers, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : index_tickers.data) {
      Statistics statistics[] = {
          {
              .type = StatisticsType::INDEX_VALUE,
              .value = item.idx_px,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
      };
      const StatisticsUpdate statistics_update{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::FundingRate> const &event) {
  profile_.funding_rate([&]() {
    auto &[trace_info, funding_rate] = event;
    log::info<3>("event={{funding_rate={}, trace_info={}}}"sv, funding_rate, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : funding_rate.data) {
      Statistics statistics[] = {
          {
              .type = StatisticsType::FUNDING_RATE,
              .value = item.funding_rate,
              .begin_time_utc = utils::safe_cast(item.funding_time),
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::FUNDING_RATE_PREDICTION,
              .value = item.next_funding_rate,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
      };
      const StatisticsUpdate statistics_update{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("event={{login={}, trace_info={}}}"sv, login, trace_info);
    log::debug("login={}"sv, login);
    (*connection_).touch(trace_info.source_receive_time);
    auto state = MarketDataState::LOGIN;
    download_.check_relaxed(state);
  });
}

void MarketData::operator()(Trace<json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::OrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::AmendOrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::CancelOrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch(
      [&](auto now) { return shared_.rate_limiter.can_request(now); },
      [&](auto &message) {
        log::debug(R"(Subscribe: "{}")"sv, message);
        (*connection_).send_text(message);
      },
      now);
}

}  // namespace okx
}  // namespace roq
