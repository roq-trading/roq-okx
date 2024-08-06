/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/okx/market_data.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/charconv.hpp"

#include "roq/core/tools/exception.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/okx/json/map.hpp"
#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "md"sv;

auto const SUPPORTS = Mask{
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

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.ws.public_uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = {},
      .uris = {&uri, 1},
      .host = settings.ws.public_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = settings.net.disconnect_on_idle_timeout,
      .always_reconnect = true,
      // proxy
      .proxy = {},
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = {},
      .ping_frequency = settings.ws.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto &settings, auto const &group, auto const &function) : core::metrics::Factory(settings.app.name, group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

MarketData::MarketData(Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, size_t index)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, index_{index}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_(shared.settings.misc.decode_buffer_size),
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
          .estimated_price = create_metrics(shared.settings, name_, "estimated_price"sv),
          .price_limit = create_metrics(shared.settings, name_, "price_limit"sv),
          .mark_price = create_metrics(shared.settings, name_, "mark_price"sv),
          .tickers = create_metrics(shared.settings, name_, "tickers"sv),
          .trades = create_metrics(shared.settings, name_, "trades"sv),
          .bbo_tbt = create_metrics(shared.settings, name_, "bbo_tbt"sv),
          .books_l2_tbt = create_metrics(shared.settings, name_, "books_l2_tbt"sv),
          .index_tickers = create_metrics(shared.settings, name_, "index_tickers"sv),
          .funding_rate = create_metrics(shared.settings, name_, "funding_rate"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      account_{account}, shared_{shared}, download_{{}, [this](auto state) { return download(state); }} {
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
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.subscribe, metrics::Type::PROFILE)
      .write(profile_.unsubscribe, metrics::Type::PROFILE)
      .write(profile_.login, metrics::Type::PROFILE)
      .write(profile_.status, metrics::Type::PROFILE)
      .write(profile_.instruments, metrics::Type::PROFILE)
      .write(profile_.estimated_price, metrics::Type::PROFILE)
      .write(profile_.price_limit, metrics::Type::PROFILE)
      .write(profile_.mark_price, metrics::Type::PROFILE)
      .write(profile_.tickers, metrics::Type::PROFILE)
      .write(profile_.trades, metrics::Type::PROFILE)
      .write(profile_.bbo_tbt, metrics::Type::PROFILE)
      .write(profile_.books_l2_tbt, metrics::Type::PROFILE)
      .write(profile_.index_tickers, metrics::Type::PROFILE)
      .write(profile_.funding_rate, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
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
  auto external_latency = ExternalLatency{
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
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
        .interface = (*connection_).get_interface(),
        .authority = (*connection_).get_current_authority(),
        .path = (*connection_).get_current_path(),
        .proxy = (*connection_).get_proxy(),
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
      if (std::empty(account_)) {
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
      return 0;
  }
  assert(false);
  return 0;
}

void MarketData::login() {
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

void MarketData::subscribe_static() {
  if (index_ != 0)  // note! only subscribe instruments from the first connection
    return;
  subscribe("status"sv);
  subscribe("instruments"sv, "instType"sv, "SPOT"sv);
  subscribe("instruments"sv, "instType"sv, "SWAP"sv);
  subscribe("instruments"sv, "instType"sv, "FUTURES"sv);
  // subscribe("instruments"sv, "instType"sv, "OPTION"sv);
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
  auto get_books_channel = [this](auto account) {
    std::string_view result;
    auto vip = account && !shared_.settings.ws.books_use_public;
    switch (shared_.settings.ws.books_depth) {
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
        log::fatal("Unsupported --ws_books_depth={}"sv, shared_.settings.ws.books_depth);
    }
    log::info(R"(DEBUG: using channel="{}")"sv, result);
    return result;
  };
  subscribe(get_books_channel(!std::empty(account_)), "instId"sv, symbols);
  for (auto &symbol : symbols) {
    if (shared_.settings.misc.include_bad_subscriptions ||
        shared_.extended_symbols.find(static_cast<std::string_view>(symbol)) != shared_.extended_symbols.end()) {
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
  subscribe_queue_.emplace_back(message);
}

void MarketData::subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
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

void MarketData::subscribe(std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values) {
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

void MarketData::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!json::Parser::dispatch(*this, message, decode_buffer_, trace_info))
        log_message();
    } catch (...) {
      log_message();
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
    if (subscribe.channel == json::Channel::INSTRUMENTS && subscribe.inst_type == "FUTURES"sv) {
      log::info("Request instruments..."sv);
      shared_.instruments.request = clock::get_system();
    }
  });
}

void MarketData::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
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
      auto reference_data = ReferenceData{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = symbol,
          .description = {},
          .security_type = json::Map{item.inst_type},
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
          .option_type = json::Map{item.opt_type},
          .strike_currency = {},
          .strike_price = item.stk,
          .underlying = item.uly,
          .time_zone = {},
          .issue_date = utils::safe_cast(item.list_time),
          .settlement_date = {},
          .expiry_datetime = utils::safe_cast(item.exp_time),
          .expiry_datetime_utc = utils::safe_cast(item.exp_time),
          .exchange_time_utc = {},
          .exchange_sequence = {},
          .sending_time_utc = {},
          .discard = discard,
      };
      create_trace_and_dispatch(handler_, trace_info, reference_data, true);
      if (discard)
        continue;
      if (shared_.all_symbols.emplace(symbol).second)  // only include new
        symbols.emplace_back(symbol);
      ++counter;
      auto market_status = MarketStatus{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .trading_status = json::Map{item.state},
          .exchange_time_utc = {},
          .exchange_sequence = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, market_status, true);
      // trying to reduce the number of symbols where we next extra subscriptions
      // but still avoid not reducing too much
      switch (item.inst_type) {
        using enum json::InstrumentType::type_t;
        case UNDEFINED__:
        case UNKNOWN__:
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
      auto symbols_update = SymbolsUpdate{
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
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::PriceLimit> const &event) {
  profile_.price_limit([&]() {
    auto &[trace_info, price_limit] = event;
    log::info<3>("event={{price_limit={}, trace_info={}}}"sv, price_limit, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::MarkPrice> const &event) {
  profile_.mark_price([&]() {
    auto &[trace_info, mark_price] = event;
    log::info<3>("event={{mark_price={}, trace_info={}}}"sv, mark_price, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::Tickers> const &event) {
  profile_.tickers([&]() {
    auto &[trace_info, tickers] = event;
    log::info<3>("event={{tickers={}, trace_info={}}}"sv, tickers, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : tickers.data) {
      std::array<Statistics, 4> statistics{{
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
      }};
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = item.ts,
          .exchange_sequence = {},
          .sending_time_utc = {},
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
    shared_.trades.clear();
    auto emplace_back = [](auto &result, auto &value) {
      auto trade = Trade{
          .side = json::Map{value.side},
          .price = value.px,
          .quantity = value.sz,
          .trade_id = value.trade_id,
          .taker_order_id = {},
          .maker_order_id = {},
      };
      result.emplace_back(std::move(trade));
    };
    std::string_view symbol;
    std::chrono::nanoseconds exchange_time_utc = {};
    for (auto &item : trades.data) {
      if (utils::update(symbol, item.inst_id) && !std::empty(shared_.trades)) {
        assert(!std::empty(symbol));
        auto trade_summary = TradeSummary{
            .stream_id = stream_id_,
            .exchange = shared_.settings.exchange,
            .symbol = symbol,
            .trades = shared_.trades,
            .exchange_time_utc = exchange_time_utc,
            .exchange_sequence = {},
            .sending_time_utc = {},
        };
        create_trace_and_dispatch(handler_, trace_info, trade_summary, false);
        shared_.trades.clear();
        exchange_time_utc = {};
      }
      utils::update_max(exchange_time_utc, item.ts);
      emplace_back(shared_.trades, item);
    }
    if (!std::empty(shared_.trades)) {
      assert(!std::empty(symbol));
      auto trade_summary = TradeSummary{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = symbol,
          .trades = shared_.trades,
          .exchange_time_utc = exchange_time_utc,
          .exchange_sequence = {},
          .sending_time_utc = {},
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
    auto top_of_book = TopOfBook{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = inst_id,
        .layer{
            .bid_price = std::empty(bids) ? NaN : bids[0].price,
            .bid_quantity = std::empty(bids) ? NaN : bids[0].size,
            .ask_price = std::empty(asks) ? NaN : asks[0].price,
            .ask_quantity = std::empty(asks) ? NaN : asks[0].size,
        },
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = bbo_tbt.ts,
        .exchange_sequence = bbo_tbt.seq_id,
        .sending_time_utc = {},
    };
    create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
  });
}

void MarketData::operator()(Trace<json::BooksL2Tbt> const &event, std::string_view const &inst_id, json::Action action) {
  profile_.books_l2_tbt([&]() {
    auto &[trace_info, books_l2_tbt] = event;
    log::info<3>("event={{books_l2_tbt={}, action={}, trace_info={}}}"sv, books_l2_tbt, action, trace_info);
    (*connection_).touch(trace_info.source_receive_time);
    auto snapshot = action == json::Action::SNAPSHOT;
    shared_.bids.clear();
    shared_.asks.clear();
    auto emplace_back = [](auto &result, auto &item) {
      auto mbp_update = MBPUpdate{
          .price = item.price,
          .quantity = item.size,
          .implied_quantity = NaN,
          .number_of_orders = utils::safe_cast(std::min(item.orders, 65535u)),
          .update_action = {},
          .price_level = {},
      };
      result.emplace_back(std::move(mbp_update));
    };
    for (auto &item : books_l2_tbt.bids)
      emplace_back(shared_.bids, item);
    for (auto &item : books_l2_tbt.asks)
      emplace_back(shared_.asks, item);
    // XXX HANS validate checksum
    auto update_type = snapshot ? UpdateType::SNAPSHOT : UpdateType::INCREMENTAL;
    auto market_by_price_update = MarketByPriceUpdate{
        .stream_id = stream_id_,
        .exchange = shared_.settings.exchange,
        .symbol = inst_id,
        .bids = shared_.bids,
        .asks = shared_.asks,
        .update_type = update_type,
        .exchange_time_utc = books_l2_tbt.ts,
        .exchange_sequence = books_l2_tbt.seq_id,
        .sending_time_utc = {},
        .price_precision = {},
        .quantity_precision = {},
        .checksum = {},
    };
    log::info<3>("market_by_price_update={}"sv, market_by_price_update);
    try {
      create_trace_and_dispatch(handler_, trace_info, market_by_price_update, true);
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
      auto statistics = Statistics{
          .type = StatisticsType::INDEX_VALUE,
          .value = item.idx_px,
          .begin_time_utc = {},
          .end_time_utc = {},
      };
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = {&statistics, 1u},
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = {},
          .exchange_sequence = {},
          .sending_time_utc = {},
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
      std::array<Statistics, 2> statistics{{
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
      }};
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = {},
          .exchange_sequence = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::ChannelConnCount> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("event={{login={}, trace_info={}}}"sv, login, trace_info);
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

// request

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch([&](auto now) { return shared_.rate_limiter.can_request(now); }, [&](auto &message) { (*connection_).send_text(message); }, now);
}

}  // namespace okx
}  // namespace roq
