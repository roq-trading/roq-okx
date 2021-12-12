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
    Handler &handler, core::io::Context &context, uint32_t stream_id, Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
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
          .tickers = create_metrics(name_, "tickers"sv),
          .trades = create_metrics(name_, "trades"sv),
          .books_l2_tbt = create_metrics(name_, "books_l2_tbt"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_(shared), download_({}, [this](auto state) { return download(state); }) {
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
  /*
  if (connection_.ready()) {
    if (welcome_ && next_ping_ < now)
      send_ping(now);
    check_subscribe_queue(now);
  } else if (logon_timeout_.count() && logon_timeout_ < now) {
    assert(!welcome_);
    log::warn("Did not receive the welcome message, disconnecting now..."sv);
    connection_.close();
  }
  */
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
      .write(profile_.tickers, metrics::PROFILE)
      .write(profile_.trades, metrics::PROFILE)
      .write(profile_.books_l2_tbt, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketData::update_subscriptions(std::vector<std::string> &symbols) {
  assert(&symbols != &symbols_);
  auto max_size = Flags::ws_max_subscriptions_per_stream();
  auto offset = std::size(symbols_);
  if (max_size <= offset)
    return;
  if (std::empty(symbols))
    return;
  symbols_.reserve(max_size);
  auto length = std::min(max_size - offset, std::size(symbols));
  assert(length > 0);
  for (size_t i = 0; i < length; ++i) {
    symbols_.emplace_back(symbols.back());
    symbols.pop_back();
  }
  assert(length == (std::size(symbols_) - offset));
  if (ready())
    subscribe({&symbols_[offset], length});
}

void MarketData::operator()(const core::web::ClientSocket::Connected &) {
}

void MarketData::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  // experimental
  shared_.mbp_collector.clear();  // XXX HANS this is SHARED !!!
}

void MarketData::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
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
  /*
  if (inflate_.decode(binary.payload, shared_.generic_buffer, [this](auto &payload) {
        std::string_view message{
            reinterpret_cast<char *const>(std::data(payload)), std::size(payload)};
        log::info<5>(R"(message="{}")"sv, message);
        parse(message);
      })) {
  } else {
    log::fatal("Unexpected: incomplete zlib message"sv);
  }
  */
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

uint32_t MarketData::download(MarketDataState state) {
  switch (state) {
    case MarketDataState::UNDEFINED:
      assert(false);
      break;
    case MarketDataState::SUBSCRIBE:
      subscribe(symbols_);
      return {};
    case MarketDataState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

void MarketData::subscribe(const roq::span<std::string> &symbols) {
  if (std::empty(symbols))
    return;
  subscribe("tickers"sv, symbols);
  subscribe("trades"sv, symbols);
  subscribe("books-l2-tbt"sv, symbols);
}

void MarketData::subscribe(const std::string_view &channel, const roq::span<std::string> &symbols) {
  assert(!std::empty(symbols));
  auto prefix = fmt::format(
      R"({{)"
      R"("channel":"{}",)"
      R"("instId":")"sv,
      channel);
  auto separator = fmt::format(R"("}},{})", prefix);
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[)"
      R"({}{}"}})"
      R"(])"
      R"(}})"sv,
      prefix,
      fmt::join(symbols, separator));
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void MarketData::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
      json::Parser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void MarketData::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::info<1>("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void MarketData::operator()(server::Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
  });
}

void MarketData::operator()(server::Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
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
    // XXX HANS proper would be to aggregate the trades
    if (std::size(trades.data) > 1) {
      log::fatal("{}"sv, trades);
    }
    for (auto &item : trades.data) {
      Trade trade{
          .side = json::map(item.side),
          .price = item.px,
          .quantity = item.sz,
          .trade_id = item.trade_id,
      };
      const TradeSummary trade_summary{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .trades = {&trade, 1},
          .exchange_time_utc = utils::safe_cast(item.ts),
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

}  // namespace okex
}  // namespace roq
