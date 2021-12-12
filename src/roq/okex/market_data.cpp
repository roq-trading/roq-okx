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
          core::URI{Flags::ws_uri()},
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
          .spot_ticker = create_metrics(name_, "spot_ticker"sv),
          .spot_trade = create_metrics(name_, "spot_trade"sv),
          .spot_depth_l2_tbt = create_metrics(name_, "spot_depth_l2_tbt"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_(shared), download_({}, [this](auto state) { return download(state); }),
      inflate_(-MAX_WBITS) {
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
      .write(profile_.spot_ticker, metrics::PROFILE)
      .write(profile_.spot_trade, metrics::PROFILE)
      .write(profile_.spot_depth_l2_tbt, metrics::PROFILE)
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
  log::fatal(R"(Unexpected: message="{}")"sv);
  parse(text.payload);
}

void MarketData::operator()(const core::web::ClientSocket::Binary &binary) {
  if (inflate_.decode(binary.payload, shared_.generic_buffer, [this](auto &payload) {
        std::string_view message{
            reinterpret_cast<char *const>(std::data(payload)), std::size(payload)};
        log::info<5>(R"(message="{}")"sv, message);
        parse(message);
      })) {
  } else {
    log::fatal("Unexpected: incomplete zlib message"sv);
  }
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
  subscribe("spot/ticker"sv, symbols);
  subscribe("spot/trade"sv, symbols);
  subscribe("spot/depth_l2_tbt"sv, symbols);
}

void MarketData::subscribe(const std::string_view &channel, const roq::span<std::string> &symbols) {
  assert(!std::empty(symbols));
  // note! can't exceed 4k bytes
  auto separator = fmt::format(R"(","{}:)", channel);
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":["{}:{}"])"
      R"(}})"sv,
      channel,
      fmt::join(symbols, separator));
  log::debug("message={}"sv, message);
  if (std::size(message) > 4096)
    log::fatal("{}"sv, std::size(message));
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

void MarketData::operator()(server::Trace<json::SpotTicker> const &event) {
  profile_.spot_ticker([&]() {
    auto &[trace_info, spot_ticker] = event;
    log::info<3>("event={{trace_info={}, spot_ticker={}}}"sv, trace_info, spot_ticker);
    const TopOfBook top_of_book{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = spot_ticker.instrument_id,
        .layer{
            .bid_price = spot_ticker.best_bid,
            .bid_quantity = spot_ticker.best_bid_size,
            .ask_price = spot_ticker.best_ask,
            .ask_quantity = spot_ticker.best_ask_size,
        },
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(spot_ticker.timestamp),
    };
    server::create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
    Statistics statistics[] = {
        {
            .type = StatisticsType::OPEN_PRICE,
            .value = spot_ticker.open_24h,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::HIGHEST_TRADED_PRICE,
            .value = spot_ticker.high_24h,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::LOWEST_TRADED_PRICE,
            .value = spot_ticker.low_24h,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::TRADE_VOLUME,
            .value = spot_ticker.base_volume_24h,  // note! not sure...
            .begin_time_utc = {},
            .end_time_utc = {},
        },
    };
    const StatisticsUpdate statistics_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = spot_ticker.instrument_id,
        .statistics = statistics,
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(spot_ticker.timestamp),
    };
    server::create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
  });
}

void MarketData::operator()(server::Trace<json::SpotTrade> const &event) {
  profile_.spot_trade([&]() {
    auto &[trace_info, spot_trade] = event;
    log::info<3>("event={{trace_info={}, spot_trade={}}}"sv, trace_info, spot_trade);
    Trade trade{
        .side = json::map(spot_trade.side),
        .price = spot_trade.price,
        .quantity = spot_trade.size,
        .trade_id = spot_trade.trade_id,
    };
    const TradeSummary trade_summary{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = spot_trade.instrument_id,
        .trades = {&trade, 1},
        .exchange_time_utc = utils::safe_cast(spot_trade.timestamp),
    };
    server::create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
  });
}

void MarketData::operator()(server::Trace<json::SpotDepthL2Tbt> const &event, json::Action action) {
  profile_.spot_depth_l2_tbt([&]() {
    auto &[trace_info, spot_depth_l2_tbt] = event;
    log::info<3>(
        "event={{trace_info={}, spot_depth_l2_tbt={}, action={}}}"sv,
        trace_info,
        spot_depth_l2_tbt,
        action);
    auto snapshot = action == json::Action::PARTIAL;
    core::back_emplacer bids(shared_.bids), asks(shared_.asks);
    for (auto &item : spot_depth_l2_tbt.bids)
      bids.emplace_back([&item](auto &result) { emplace(result, item); });
    for (auto &item : spot_depth_l2_tbt.asks)
      asks.emplace_back([&item](auto &result) { emplace(result, item); });
    // XXX HANS validate checksum
    const MarketByPriceUpdate market_by_price_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = spot_depth_l2_tbt.instrument_id,
        .bids = bids,
        .asks = asks,
        .update_type = snapshot ? UpdateType::SNAPSHOT : UpdateType::INCREMENTAL,
        .exchange_time_utc = spot_depth_l2_tbt.timestamp,
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
