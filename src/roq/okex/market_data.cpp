/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/market_data.h"

#include <algorithm>

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/tools/exception.h"

#include "roq/core/metrics/factory.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static const auto NAME = "md"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

template <typename T>
void emplace(Trade &result, const T &value) {
  new (&result) Trade{
      .side = json::map(value.side),
      .price = value.px,
      .quantity = value.sz,
      .trade_id = value.trade_id,
  };
}

template <typename T>
void emplace(MBPUpdate &result, const T &item) {
  new (&result) MBPUpdate{
      .price = item.price,
      .quantity = item.size,
      .implied_quantity = NaN,
      .price_level = {},
      .number_of_orders = item.orders,
  };
}
}  // namespace

MarketData::MarketData(
    Handler &handler, core::io::Context &context, uint32_t stream_id, Shared &shared, size_t index)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      index_(index), connection_(
                         *this,
                         context,
                         core::URI{Flags::ws_public_uri()},
                         {},  // query
                         Flags::ws_ping_freq(),
                         Flags::decode_buffer_size(),
                         Flags::encode_buffer_size(),
                         []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      request_id_(static_cast<uint64_t>(stream_id_) * 1000000),  // scale (debugging)
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .error = create_metrics(name_, "error"sv),
          .subscribe = create_metrics(name_, "subscribe"sv),
          .unsubscribe = create_metrics(name_, "unsubscribe"sv),
          .status = create_metrics(name_, "status"sv),
          .instruments = create_metrics(name_, "instruments"sv),
          .estimated_price = create_metrics(name_, "estimated_price"sv),
          .price_limit = create_metrics(name_, "price_limit"sv),
          .mark_price = create_metrics(name_, "mark_price"sv),
          .tickers = create_metrics(name_, "tickers"sv),
          .trades = create_metrics(name_, "trades"sv),
          .books_l2_tbt = create_metrics(name_, "books_l2_tbt"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_(shared) {
}

void MarketData::operator()(const Event<Start> &) {
  connection_.start();
}

void MarketData::operator()(const Event<Stop> &) {
  connection_.stop();
}

void MarketData::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
  if (connection_.ready())
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
      .write(profile_.status, metrics::PROFILE)
      .write(profile_.instruments, metrics::PROFILE)
      .write(profile_.estimated_price, metrics::PROFILE)
      .write(profile_.price_limit, metrics::PROFILE)
      .write(profile_.mark_price, metrics::PROFILE)
      .write(profile_.tickers, metrics::PROFILE)
      .write(profile_.trades, metrics::PROFILE)
      .write(profile_.books_l2_tbt, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketData::subscribe(size_t start_from) {
  if (ready())
    subscribe(shared_.symbols.get_slice(index_, start_from));
}

void MarketData::operator()(const core::web::ClientSocket::Connected &) {
}

void MarketData::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  subscribe_queue_.clear();
}

void MarketData::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::READY);
  subscribe_static();
  subscribe();
}

void MarketData::operator()(const core::web::ClientSocket::Close &) {
}

void MarketData::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(const core::web::ClientSocket::Text &text) {
  parse(text.payload);
}

void MarketData::operator()(const core::web::ClientSocket::Binary &) {
  log::fatal("Unexpected: binary"sv);
}

void MarketData::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::WEB_SOCKET,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
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

void MarketData::subscribe(const std::span<std::string const> &symbols) {
  if (std::empty(symbols))
    return;
  // subscribe("price-limit"sv, "instType"sv, symbols);
  // subscribe("mark-price"sv, "instType"sv, symbols);
  subscribe("tickers"sv, "instId"sv, symbols);
  subscribe("trades"sv, "instId"sv, symbols);
  subscribe("books-l2-tbt"sv, "instId"sv, symbols);
}

void MarketData::subscribe(const std::string_view &channel) {
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
    const std::string_view &channel,
    const std::string_view &selector,
    const std::string_view &value) {
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
    const std::string_view &channel,
    const std::string_view &selector,
    const std::span<std::string const> &values) {
  assert(!std::empty(values));
  auto prefix = fmt::format(
      R"({{)"
      R"("channel":"{}",)"
      R"("{}":")"sv,
      channel,
      selector);
  auto separator = fmt::format(R"("}},{})", prefix);
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

void MarketData::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      // log::debug(R"(message="{}")"sv, message);
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
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

void MarketData::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void MarketData::operator()(server::Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
    log::debug("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
  });
}

void MarketData::operator()(server::Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
    log::debug("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
  });
}

void MarketData::operator()(server::Trace<json::Status> const &event) {
  profile_.status([&]() {
    auto &[trace_info, status] = event;
    log::info<3>("event={{trace_info={}, status={}}}"sv, trace_info, status);
    log::debug("event={{trace_info={}, status={}}}"sv, trace_info, status);
  });
}

void MarketData::operator()(server::Trace<json::Instruments> const &event) {
  profile_.instruments([&]() {
    auto &[trace_info, instruments] = event;
    log::info<1>("event={{trace_info={}, instruments={}}}"sv, trace_info, instruments);
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
          .security_type = json::map(item.inst_type),
          .base_currency = item.base_ccy,
          .quote_currency = item.quote_ccy,
          .margin_currency = item.settle_ccy,
          .commission_currency = {},
          .tick_size = item.tick_sz,
          .multiplier = item.ct_mult,
          .min_trade_vol = item.min_sz,
          .max_trade_vol = NaN,
          .trade_vol_step_size = NaN,
          .option_type = json::map(item.opt_type),
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
          .trading_status = json::map(item.state),
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
  });
}

void MarketData::operator()(server::Trace<json::EstimatedPrice> const &event) {
  profile_.estimated_price([&]() {
    auto &[trace_info, estimated_price] = event;
    log::info<3>("event={{trace_info={}, estimated_price={}}}"sv, trace_info, estimated_price);
    log::debug("event={{trace_info={}, estimated_price={}}}"sv, trace_info, estimated_price);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(server::Trace<json::PriceLimit> const &event) {
  profile_.price_limit([&]() {
    auto &[trace_info, price_limit] = event;
    log::info<3>("event={{trace_info={}, price_limit={}}}"sv, trace_info, price_limit);
    log::debug("event={{trace_info={}, price_limit={}}}"sv, trace_info, price_limit);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(server::Trace<json::MarkPrice> const &event) {
  profile_.mark_price([&]() {
    auto &[trace_info, mark_price] = event;
    log::info<3>("event={{trace_info={}, mark_price={}}}"sv, trace_info, mark_price);
    log::debug("event={{trace_info={}, mark_price={}}}"sv, trace_info, mark_price);
    log::fatal("here"sv);
  });
}

void MarketData::operator()(server::Trace<json::Tickers> const &event) {
  profile_.tickers([&]() {
    auto &[trace_info, tickers] = event;
    log::info<3>("event={{trace_info={}, tickers={}}}"sv, trace_info, tickers);
    for (auto &item : tickers.data) {
      const TopOfBook top_of_book{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .layer{
              .bid_price = item.bid_px,
              .bid_quantity = item.bid_sz,
              .ask_price = item.ask_px,
              .ask_quantity = item.ask_sz,
          },
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = utils::safe_cast(item.ts),
      };
      server::create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
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
      server::create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(server::Trace<json::Trades> const &event) {
  profile_.trades([&]() {
    auto &[trace_info, trades] = event;
    log::info<3>("event={{trace_info={}, trades={}}}"sv, trace_info, trades);
    core::back_emplacer trades_(shared_.trades);
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
        };
        server::create_trace_and_dispatch(handler_, trace_info, trade_summary, false);
        trades_.clear();
        exchange_time_utc = {};
      }
      utils::update_max(exchange_time_utc, item.ts);
      trades_.emplace_back([&item](auto &result) { emplace(result, item); });
    }
    if (!std::empty(trades_)) {
      assert(!std::empty(symbol));
      const TradeSummary trade_summary{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = symbol,
          .trades = trades_,
          .exchange_time_utc = exchange_time_utc,
      };
      server::create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
    }
  });
}

void MarketData::operator()(
    server::Trace<json::BooksL2Tbt> const &event,
    const std::string_view &inst_id,
    json::Action action) {
  profile_.books_l2_tbt([&]() {
    auto &[trace_info, books_l2_tbt] = event;
    log::info<3>(
        "event={{trace_info={}, books_l2_tbt={}, action={}}}"sv, trace_info, books_l2_tbt, action);
    auto snapshot = action == json::Action::SNAPSHOT;
    core::back_emplacer bids(shared_.bids), asks(shared_.asks);
    for (auto &item : books_l2_tbt.bids)
      bids.emplace_back([&item](auto &result) { emplace(result, item); });
    for (auto &item : books_l2_tbt.asks)
      asks.emplace_back([&item](auto &result) { emplace(result, item); });
    // XXX HANS validate checksum
    const MarketByPriceUpdate market_by_price_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = inst_id,
        .bids = bids,
        .asks = asks,
        .update_type = snapshot ? UpdateType::SNAPSHOT : UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(books_l2_tbt.ts),
        .exchange_sequence = {},
        .price_decimals = {},
        .quantity_decimals = {},
        .checksum = {},
    };
    log::info<3>("market_by_price_update={}"sv, market_by_price_update);
    try {
      server::create_trace_and_dispatch(handler_, trace_info, market_by_price_update, true, false);
    } catch (BadState &) {
      // resubscribe_order_book_l2(symbol);
    }
  });
}

void MarketData::operator()(server::Trace<json::Login> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::OrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::AmendOrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(server::Trace<json::CancelOrderAck> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch(
      [&](auto now) { return shared_.rate_limiter.can_request(now); },
      [&](auto &message) {
        log::debug(R"(Subscribe: "{}")"sv, message);
        connection_.send_text(message);
      },
      now);
}

}  // namespace okex
}  // namespace roq
