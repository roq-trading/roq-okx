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

MarketData::MarketData(
    Handler &handler,
    core::io::Context &context,
    uint32_t stream_id,
    Shared &shared,
    const std::string_view &uri,
    const std::string_view &query,
    std::chrono::nanoseconds ping_frequency)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      ping_frequency_(ping_frequency), connection_(
                                           *this,
                                           context,
                                           core::URI{uri},
                                           query,
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
          .welcome = create_metrics(name_, "welcome"sv),
          .error = create_metrics(name_, "error"sv),
          .pong = create_metrics(name_, "pong"sv),
          .ack = create_metrics(name_, "ack"sv),
          .snapshot = create_metrics(name_, "snapshot"sv),
          .ticker = create_metrics(name_, "ticker"sv),
          .level2 = create_metrics(name_, "level2"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_(shared), download_({}, [this](auto state) { return download(state); }) {
  if (ping_frequency_.count() == 0)
    log::fatal("Unexpected"sv);
  log::info("ping_frequency={}"sv, ping_frequency_);
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
  if (connection_.ready()) {
    if (welcome_ && next_ping_ < now)
      send_ping(now);
    check_subscribe_queue(now);
  } else if (logon_timeout_.count() && logon_timeout_ < now) {
    assert(!welcome_);
    log::warn("Did not receive the welcome message, disconnecting now..."sv);
    connection_.close();
  }
}

void MarketData::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.welcome, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.pong, metrics::PROFILE)
      .write(profile_.ack, metrics::PROFILE)
      .write(profile_.snapshot, metrics::PROFILE)
      .write(profile_.ticker, metrics::PROFILE)
      .write(profile_.level2, metrics::PROFILE)
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
  assert(logon_timeout_.count() == 0);
  auto now = core::get_system_clock();
  logon_timeout_ = now + Flags::ws_request_timeout();
}

void MarketData::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  welcome_ = false;
  logon_timeout_ = {};
  next_ping_ = {};
  // experimental
  shared_.mbp_collector.clear();  // XXX HANS this is SHARED !!!
}

void MarketData::operator()(const core::web::ClientSocket::Ready &) {
  // note! wait for welcome
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
  log::fatal("Unexpected"sv);
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
      assert(!ready_);
      ready_ = true;
      return {};
  }
  assert(false);
  return {};
}

void MarketData::subscribe(const roq::span<std::string> &symbols) {
  if (std::empty(symbols))
    return;
  subscribe("ticker"sv, symbols);
  subscribe("snapshot"sv, symbols);
  subscribe("level2"sv, symbols);
}

void MarketData::subscribe(const std::string_view &topic, const roq::span<std::string> &symbols) {
  assert(!std::empty(symbols));
  // note!
  // we could subscribe all in one message, but if any subject fails, the entire request fails
  for (auto &symbol : symbols) {
    auto now = core::get_system_clock();
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}-{}-{}",)"
        R"("type":"subscribe",)"
        R"("topic":"/market/{}:{}",)"
        R"("response":true)"
        R"(}})"sv,
        now.count(),
        topic,
        symbol,
        topic,
        symbol);
    log::debug("message={}"sv, message);
    subscribe_queue_.emplace_back(now, message);
  }
}

void MarketData::send_ping(std::chrono::nanoseconds now) {
  assert(ping_frequency_.count() > 0);
  next_ping_ = now + ping_frequency_ / 2;
  auto message = fmt::format(R"({{"id":{},"type":"ping"}})"sv, now.count());
  // log::debug(R"(message="{}")"sv, message);
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

void MarketData::operator()(server::Trace<json::Welcome> const &event) {
  profile_.welcome([&]() {
    auto &[trace_info, welcome] = event;
    log::info<1>("event={{trace_info={}, welcome={}}}"sv, trace_info, welcome);
    welcome_ = true;
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  });
}

void MarketData::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    // XXX HANS DEBUG
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
    // log::fatal("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void MarketData::operator()(server::Trace<json::Pong> const &event) {
  profile_.pong([&]() {
    auto &[trace_info, pong] = event;
    log::info<4>("event={{trace_info={}, pong={}}}"sv, trace_info, pong);
  });
}

void MarketData::operator()(server::Trace<json::Ack> const &event) {
  profile_.ack([&]() {
    auto &[trace_info, ack] = event;
    log::info<2>("event={{trace_info={}, ack={}}}"sv, trace_info, ack);
    log::debug("ack={}"sv, ack);
  });
}

void MarketData::operator()(server::Trace<json::Snapshot> const &event) {
  profile_.snapshot([&]() {
    auto &[trace_info, snapshot] = event;
    log::info<4>("event={{trace_info={}, snapshot={}}}"sv, trace_info, snapshot);
    auto &item = snapshot.data.data;
    Statistics statistics[] = {
        {
            .type = StatisticsType::OPEN_PRICE,
            .value = item.open,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::HIGHEST_TRADED_PRICE,
            .value = item.high,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::LOWEST_TRADED_PRICE,
            .value = item.low,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
        {
            .type = StatisticsType::CLOSE_PRICE,
            .value = item.close,
            .begin_time_utc = {},
            .end_time_utc = {},
        },
    };
    const StatisticsUpdate statistics_update{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .statistics = statistics,
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(item.datetime),
    };
    server::create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    const MarketStatus market_status{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = item.symbol,
        .trading_status = item.trading ? TradingStatus::OPEN : TradingStatus::HALT,
    };
    server::create_trace_and_dispatch(handler_, trace_info, market_status, true);
  });
}

void MarketData::operator()(server::Trace<json::Ticker> const &event) {
  profile_.ticker([&]() {
    auto &[trace_info, ticker] = event;
    log::info<4>("event={{trace_info={}, ticker={}}}"sv, trace_info, ticker);
    auto &data = ticker.data;
    auto symbol = json::strip_symbol_from_topic(ticker.topic);
    const TopOfBook top_of_book{
        .stream_id = stream_id_,
        .exchange = Flags::exchange(),
        .symbol = symbol,
        .layer{
            .bid_price = data.best_bid,
            .bid_quantity = data.best_bid_size,
            .ask_price = data.best_ask,
            .ask_quantity = data.best_ask_size,
        },
        .update_type = UpdateType::INCREMENTAL,
        .exchange_time_utc = utils::safe_cast(data.time),
    };
    server::create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
    if (std::isnan(data.price) == false && std::isnan(data.size) == false) {
      // note! what is this? accumulation of all done trades?
      Trade trade{
          .side = Side::UNDEFINED,
          .price = data.price,
          .quantity = data.size,
          .trade_id = {},
      };
      const TradeSummary trade_summary{
          .stream_id = stream_id_,
          .exchange = Flags::exchange(),
          .symbol = symbol,
          .trades = {&trade, 1},
          .exchange_time_utc = utils::safe_cast(data.time),
      };
      server::create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
    }
  });
}

void MarketData::operator()(server::Trace<json::Level2> const &event) {
  profile_.level2([&]() {
    // auto &[trace_info, level2] = event;
    auto &trace_info = event.trace_info;
    auto &level2 = event.value;
    log::info<4>("event={{trace_info={}, level2={}}}"sv, trace_info, level2);
    auto &data = level2.data;
    auto symbol = data.symbol;
    auto first_sequence = data.sequence_start;
    auto last_sequence = data.sequence_end;
    auto previous_sequence = first_sequence - 1;
    auto &collector = shared_.mbp_collector[symbol];
    core::back_emplacer bids(shared_.bids), asks(shared_.asks);
    for (auto &item : data.changes.bids) {
      if (utils::compare(item.price, 0.0) != 0) {
        bids.emplace_back([&item](auto &result) { emplace(result, item.price, item.size); });
      }
    }
    for (auto &item : data.changes.asks) {
      if (utils::compare(item.price, 0.0) != 0) {
        asks.emplace_back([&item](auto &result) { emplace(result, item.price, item.size); });
      }
    }
    try {
      collector(
          bids,
          asks,
          first_sequence,
          last_sequence,
          previous_sequence,
          [&](auto &bids, auto &asks) {  // update
            // log::debug(R"(PUBLISH UPDATE symbol="{}")"sv, symbol);
            MarketByPriceUpdate market_by_price_update{
                .stream_id = stream_id_,
                .exchange = Flags::exchange(),
                .symbol = symbol,
                .bids = bids,
                .asks = asks,
                .update_type = UpdateType::INCREMENTAL,
                .exchange_time_utc = {},
                .exchange_sequence = last_sequence,
                .price_decimals = {},
                .quantity_decimals = {},
                .checksum = {},
            };
            server::create_trace_and_dispatch(
                handler_, trace_info, market_by_price_update, true, false);
          },
          [&](auto &bids, auto &asks, auto sequence) {  // snapshot
            log::debug(R"(PUBLISH SNAPSHOT symbol="{}", sequence={})"sv, symbol, sequence);
            MarketByPriceUpdate market_by_price_update{
                .stream_id = stream_id_,
                .exchange = Flags::exchange(),
                .symbol = symbol,
                .bids = bids,
                .asks = asks,
                .update_type = UpdateType::SNAPSHOT,
                .exchange_time_utc = {},
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
  });
}

void MarketData::operator()(server::Trace<json::AccountBalance> const &) {
}

void MarketData::operator()(server::Trace<json::OrderChange> const &) {
}

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  while (!std::empty(subscribe_queue_)) {
    auto &tmp = subscribe_queue_.front();
    if (now < tmp.first)
      break;
    if (shared_.can_request(now, [&]() {
          auto &message = tmp.second;
          log::debug(R"(Subscribe: "{}")"sv, message);
          connection_.send_text(message);
          subscribe_queue_.pop_front();
        })) {
    } else {
      return;
    }
  }
}

}  // namespace okex
}  // namespace roq
