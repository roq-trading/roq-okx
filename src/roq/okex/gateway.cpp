/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/gateway.h"

#include <limits>
#include <utility>

#include "roq/core/utils.h"

#include "roq/okex/options.h"

#include "roq/okex/json/utils.h"

namespace roq {
namespace okex {

template <typename T>
static bool mbp_update(
    auto& data,
    size_t& offset,
    const T& item) {
  auto& obj = data[offset];
  new (&obj) MBPUpdate {
    .price = item.price,
    .quantity = item.size,
  };
  ++offset;
  return offset < data.size();
}

template <typename T>
static bool trade_update(
    auto& data,
    size_t& offset,
    const T& item) {
  auto& obj = data[offset];
  new (&obj) Trade {
    .side = json::map(item.side),
    .price = item.price,
    .quantity = item.quantity,
    .trade_id = {},
  };
  // XXX write tradeid
  ++offset;
  return offset < data.size();
}

Gateway::Gateway(
    server::Dispatcher& dispatcher,
    const Config& config)
    : _dispatcher(dispatcher),
      _account(config.get_account()),
      _access_key(config.get_access_key()),
      _random(config.get_access_secret()),
      _dns_base(_base, true),
      _web_socket {
        .connection = {
          *this,
          config,
          _random,
          _base,
          _dns_base,
          _ssl_context,
        },
        .download = WebSocketDownload(
            std::chrono::seconds { FLAGS_download_timeout_secs },
            [this](auto state) {
              return download(state);
            }),
      },
      _rest {
        .connection = {
          *this,
          config,
          _random,
          _base,
          _dns_base,
          _ssl_context,
        },
      },
      _bid(FLAGS_cache_mbp_max_depth),
      _ask(FLAGS_cache_mbp_max_depth),
      _trade(FLAGS_max_trades) {
  LOG_IF(WARNING, FLAGS_cancel_on_disconnect == false)(
      "Orders will *NOT* be cancelled on disconnect");
}

void Gateway::operator()(const server::StartEvent& event) {
  LOG(INFO)("Starting the gateway...");
  _web_socket.connection(event);
  _rest.connection(event);
}

void Gateway::operator()(const server::StopEvent& event) {
  LOG(INFO)("Stopping the gateway...");
  _rest.connection(event);
  _web_socket.connection(event);
}

void Gateway::operator()(const server::TimerEvent& event) {
  _web_socket.connection(event);
  _rest.connection(event);
  // download
  /*
  if (_web_socket.download.has_expired()) {
    LOG(WARNING)("WebSocket download has timed out");
    _web_socket.download.reset();
    _web_socket.connection.close();
  }
  */
  _base.loop(EVLOOP_NONBLOCK);
}

void Gateway::operator()(const server::ConnectionStatusEvent&) {
}

void Gateway::operator()(
    const CreateOrderEvent& event,
    const std::string_view& request_id,
    uint32_t gateway_order_id) {
  (void)gateway_order_id;  // avoid warning
  _web_socket.connection.new_order(
      event.create_order,
      request_id);
}

void Gateway::operator()(
    const ModifyOrderEvent& event,
    const std::string_view& request_id,
    const server::OMS_Order& order) {
  _web_socket.connection.cancel_replace_order(
      event.modify_order,
      request_id,
      order);
}

void Gateway::operator()(
    const CancelOrderEvent& event,
    const std::string_view& request_id,
    const server::OMS_Order& order) {
  (void)event;  // avoid warning
  (void)request_id;  // avoid warning
  _web_socket.connection.cancel_order(order);
}

void Gateway::operator()(metrics::Writer& writer) {
  _rest.connection(writer);
  _web_socket.connection(writer);
}

// rest

void Gateway::operator()(const Rest&) {
}

// web socket

int32_t Gateway::download(WebSocketDownload::State state) {
  if (_web_socket.connection.ready() == false)
    return -1;
  switch (state) {
    case WebSocketDownload::State::UNDEFINED:
      assert(false);
      break;
    case WebSocketDownload::State::SYMBOLS:
      download_symbols();
      return 1;
    case WebSocketDownload::State::TRADING_BALANCE:
      download_trading_balance();
      return 1;
    case WebSocketDownload::State::ORDERS:
      download_orders();
      return 1;
    case WebSocketDownload::State::DONE:
      update(GatewayStatus::READY);
      subscribe_market_data();
      return 0;
  }
  assert(false);
  return 0;
}

void Gateway::operator()(const WebSocket&) {
  if (_web_socket.connection.ready()) {
    _web_socket.download.begin();
  } else {
    _web_socket.download.reset();
    _symbols.clear();
  }
}

void Gateway::operator()(const json::Symbols& symbols) {
  assert(_symbols.empty());
  size_t count = 0;
  for (auto& item : symbols.data) {
    if (_dispatcher.discard_symbol(item.id)) {
      VLOG(1)(
          FMT_STRING(R"(Drop symbol="{}")"),
          item.id);
      continue;
    }
    ++count;
    _symbols.push_back(std::string(item.id));
    ReferenceData reference_data {
      .exchange = FLAGS_exchange,
      .symbol = item.id,
      .security_type = SecurityType::UNDEFINED,
      .currency = item.quote_currency,
      .settlement_currency = item.base_currency,
      .commission_currency = item.fee_currency,
      .tick_size = item.tick_size,
      .limit_up = std::numeric_limits<double>::quiet_NaN(),
      .limit_down = std::numeric_limits<double>::quiet_NaN(),
      .multiplier = std::numeric_limits<double>::quiet_NaN(),
      .min_trade_vol = item.quantity_increment,
      .option_type = OptionType::UNDEFINED,
      .strike_currency = std::string_view(),
      .strike_price = std::numeric_limits<double>::quiet_NaN(),
    };
    VLOG(1)(
        FMT_STRING(R"(reference_data={})"),
        reference_data);
    enqueue(
        reference_data,
        true);
  }
  VLOG(1)(
      FMT_STRING(R"(- symbols: {} (/{}))"),
      count,
      symbols.data.size());
  _web_socket.download.check(WebSocketDownload::State::SYMBOLS);
}

void Gateway::operator()(const json::TradingBalance& trading_balance) {
  size_t count = 0;
  for (auto& item : trading_balance.data) {
    // XXX filter?
    ++count;
    FundsUpdate funds_update {
      .account = _account,
      .currency = item.currency,
      .balance = item.available,
      .hold = item.reserved,
    };
    VLOG(1)(
        FMT_STRING(R"(funds_update={})"),
        funds_update);
    enqueue(
        funds_update,
        true);
  }
  VLOG(1)(
      FMT_STRING(R"(- currencies: {} (/{}))"),
      count,
      trading_balance.data.size());
  _web_socket.download.check(WebSocketDownload::State::TRADING_BALANCE);
}

void Gateway::operator()(const json::Orders& orders) {
  size_t count = 0;
  for (auto& item : orders.data) {
    ++count;
    // XXX ???
  }
  VLOG(1)(
      FMT_STRING(R"(- orders: {} (/{}))"),
      count,
      orders.data.size());
  _web_socket.download.check(WebSocketDownload::State::ORDERS);
}

void Gateway::operator()(const json::Order& order) {
}

void Gateway::operator()(const json::Ticker& ticker) {
  TopOfBook top_of_book {
    .exchange = FLAGS_exchange,
    .symbol = ticker.symbol,
    .layer = {
      .bid_price = ticker.bid,
      .bid_quantity = std::numeric_limits<double>::quiet_NaN(),
      .ask_price = ticker.ask,
      .ask_quantity = std::numeric_limits<double>::quiet_NaN(),
    },
    .snapshot = false,
    .exchange_time_utc = ticker.timestamp,
  };
  enqueue(
      top_of_book,
      true);
}

void Gateway::operator()(const json::Trades& trades) {
  std::chrono::nanoseconds timestamp = {};
  size_t trade_length = 0;
  bool success = true;
  for (auto& item : trades.data) {
    if (success == false)
      break;
    success = trade_update(
        _trade,
        trade_length,
        item);
    timestamp = std::max(
        timestamp,
        item.timestamp);
  }
  if (ROQ_PREDICT_FALSE(success == false)) {
  LOG(FATAL)(
      FMT_STRING(
          R"(Insufficient trade array size(s): )"
          R"(len(trade)={}/{})"),
      trade_length, _trade.size());
  }
  if (trade_length > 0) {
    TradeSummary trade_summary {
      .exchange = FLAGS_exchange,
      .symbol = trades.symbol,
      .trades = {
        .items = _trade.data(),
        .length = trade_length,
      },
      .exchange_time_utc = timestamp,
    };
    enqueue(
        trade_summary,
        true);
  }
}

void Gateway::operator()(
    const json::Orderbook& orderbook,
    bool snapshot) {
  size_t bid_length = 0, ask_length = 0;
  bool success = true;
  for (auto& item : orderbook.bid) {
    if (success == false)
      break;
    success = mbp_update(
        _bid,
        bid_length,
        item);
  }
  for (auto& item : orderbook.ask) {
    if (success == false)
      break;
    success = mbp_update(
        _ask,
        ask_length,
        item);
  }
  if (ROQ_PREDICT_FALSE(success == false)) {
  LOG(FATAL)(
      FMT_STRING(
          R"(Insufficient bid/ask array size(s): )"
          R"(len(bid)={}/{} )"
          R"(len(ask)={}/{})"),
      bid_length, _bid.size(),
      ask_length, _ask.size());
  }
  if ((bid_length + ask_length) > 0) {
    MarketByPrice market_by_price {
      .exchange = FLAGS_exchange,
      .symbol = orderbook.symbol,
      .bids = {
        .items = _bid.data(),
        .length = bid_length,
      },
      .asks = {
        .items = _ask.data(),
        .length = ask_length,
      },
      .snapshot = snapshot,
      .exchange_time_utc = orderbook.timestamp
    };
    enqueue(
        market_by_price,
        true);
  }
}

void Gateway::update(GatewayStatus gateway_status) {
  if (gateway_status == _gateway_status)
    return;
  _gateway_status = gateway_status;
  MarketDataStatus market_data_status {
    .status = _gateway_status,
  };
  enqueue(
      market_data_status,
      false);
  OrderManagerStatus order_manager_status {
    .account = _account,
    .status = _gateway_status,
  };
  enqueue(
      order_manager_status,
      true);
  LOG(INFO)(
      FMT_STRING(R"(Update: gateway_status={})"),
      _gateway_status);
}

void Gateway::download_symbols() {
  _web_socket.connection.get_symbols();
}

void Gateway::download_trading_balance() {
  _web_socket.connection.get_trading_balance();
}

void Gateway::download_orders() {
  _web_socket.connection.get_orders();
}

void Gateway::subscribe_market_data() {
  assert(_gateway_status == GatewayStatus::READY);
  if (_symbols.empty()) {
    LOG(WARNING)("Can't subscribe market data, reason: NO SYMBOLS");
    return;
  }
  LOG(INFO)("Subscribe market data");
  for (auto& item : _symbols) {
    _web_socket.connection.subscribe_ticker(item);
    _web_socket.connection.subscribe_trades(item);
    _web_socket.connection.subscribe_orderbook(item);
  }
}

template <typename T>
inline void Gateway::enqueue(
    const T& value,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      value,
      now,
      now,
      is_last);
}

template <typename T>
inline void Gateway::enqueue(
    uint8_t user_id,
    const T& value,
    bool is_last) {
  auto now = core::get_system_clock();
  _dispatcher(
      user_id,
      value,
      now,
      now,
      is_last);
}

}  // namespace okex
}  // namespace roq
