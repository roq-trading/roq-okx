/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <string>
#include <vector>

#include "roq/download.h"
#include "roq/server.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/okex/config.h"
#include "roq/okex/random.h"
#include "roq/okex/rest.h"
#include "roq/okex/web_socket.h"

#include "roq/okex/web_socket_state.h"

#include "roq/okex/json/order.h"
#include "roq/okex/json/orders.h"
#include "roq/okex/json/symbols.h"
#include "roq/okex/json/trading_balance.h"

namespace roq {
namespace okex {

class Gateway final : public server::Handler {
 public:
  Gateway(server::Dispatcher &dispatcher, const Config &config);

  void operator()(const Event<Start> &) override;
  void operator()(const Event<Stop> &) override;
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connection> &) override;

  void operator()(
      const Event<CreateOrder> &event,
      const std::string_view &request_id,
      uint32_t gateway_order_id) override;
  void operator()(
      const Event<ModifyOrder> &event,
      const std::string_view &request_id,
      const server::OMS_Order &order) override;
  void operator()(
      const Event<CancelOrder> &event,
      const std::string_view &request_id,
      const server::OMS_Order &order) override;

  void operator()(metrics::Writer &writer) override;

  // rest
  void operator()(const Rest &);

  // web socket
  void operator()(const WebSocket &);
  // ... request
  void operator()(const json::Symbols &symbols);
  void operator()(const json::TradingBalance &trading_balance);
  void operator()(const json::Orders &orders);
  void operator()(const json::Order &order);
  // ... notification
  void operator()(const json::Ticker &ticker);
  void operator()(const json::Trades &trades);
  void operator()(const json::Orderbook &orderbook, bool snapshot);

 private:
  using WebSocketDownload = server::Download<WebSocketState>;

  int32_t download(WebSocketDownload::State state);

 private:
  void update(GatewayStatus gateway_status);

  void download_symbols();
  void download_trading_balance();
  void download_orders();

  void subscribe_market_data();

 private:
  server::Dispatcher &_dispatcher;
  // config
  const std::string _account;
  const std::string _access_key;
  // authentication
  Random _random;
  // async
  core::event::Base _base;
  core::event::DNSBase _dns_base;
  // crypto
  core::ssl::Context _ssl_context;
  // connections
  struct {
    WebSocket connection;
    WebSocketDownload download;
  } _web_socket;
  struct {
    Rest connection;
  } _rest;
  // download (web socket)
  std::vector<std::string> _symbols;
  // market data + order manager
  GatewayStatus _gateway_status = GatewayStatus::DISCONNECTED;
  // market data
  core::page_aligned_vector<MBPUpdate> _bid, _ask;
  core::page_aligned_vector<Trade> _trade;
};

}  // namespace okex
}  // namespace roq
