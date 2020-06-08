/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/event/base.h"
#include "roq/core/event/dns_base.h"

#include "roq/core/ssl/ssl.h"

#include "roq/core/web/socket.h"

#include "roq/core/jsonrpc/parser.h"

#include "roq/server.h"

#include "roq/okex/config.h"
#include "roq/okex/random.h"

#include "roq/okex/json/orderbook.h"
#include "roq/okex/json/order.h"
#include "roq/okex/json/orders.h"
#include "roq/okex/json/symbols.h"
#include "roq/okex/json/ticker.h"
#include "roq/okex/json/trades.h"
#include "roq/okex/json/trading_balance.h"

namespace roq {
namespace okex {

class Gateway;

class WebSocket final
    : public core::web::Socket::Handler,
      public core::jsonrpc::Parser::Handler {

 public:
  WebSocket(
      Gateway& gateway,
      const Config& config,
      Random& random,
      core::event::Base& base,
      core::event::DNSBase& dns_base,
      core::ssl::Context& ssl_context);

  WebSocket(WebSocket&&) = delete;
  WebSocket(const WebSocket&) = delete;

  bool ready() const;

  void close();

  void operator()(const server::StartEvent&);
  void operator()(const server::StopEvent&);
  void operator()(const server::TimerEvent&);

  // request

  void login();

  void get_symbols();
  void get_trading_balance();
  void get_orders();

  void new_order(
      const CreateOrder& create_order,
      const std::string_view& request_id);

  void cancel_replace_order(
      const ModifyOrder& modify_order,
      const std::string_view& request_id,
      const server::OMS_Order& order);

  void cancel_order(const server::OMS_Order& order);

  // subscribe

  void subscribe_ticker(const std::string_view& symbol);
  void subscribe_trades(const std::string_view& symbol);
  void subscribe_orderbook(const std::string_view& symbol);

  void operator()(metrics::Writer& writer);

 protected:
  void operator()(const core::web::Socket::Connected&) override;
  void operator()(const core::web::Socket::Disconnected&) override;
  void operator()(const core::web::Socket::Ready&) override;
  void operator()(const core::web::Socket::Close&) override;
  void operator()(const core::web::Socket::Latency&) override;
  void operator()(const core::web::Socket::Text&) override;

  void parse(const std::string_view& message);

  void operator()(
      const core::jsonrpc::Error& error,
      core::json::value_t& value) override;
  void operator()(
      const core::jsonrpc::Result& result,
      core::json::value_t& value) override;
  void operator()(
      const core::jsonrpc::Notification& notification,
      core::json::value_t& value) override;

  // response
  void operator()(const json::Symbols& symbols);
  void operator()(const json::TradingBalance& trading_balance);
  void operator()(const json::Orders& orders);
  void operator()(const json::Order& order);

  // notifications
  void operator()(const json::Ticker& ticker);
  void operator()(
      const json::Trades& trades,
      bool snapshot);
  void operator()(
      const json::Orderbook& orderbook,
      bool snapshot);

  void reset();

 private:
  Gateway& _gateway;
  // config
  const std::string _access_key;
  // authentication
  Random& _random;
  // web socket
  core::web::Socket _connection;
  // buffers
  core::utils::Buffer _decode_buffer;
  core::stack::Buffer<char, 32> _stack_buffer;
  // metrics
  struct {
    core::metrics::Counter
      disconnect;
  } _counter;
  struct {
    core::metrics::Profile
      parse,
      get_symbols,
      get_trading_balance,
      get_orders,
      order,
      ticker,
      trades,
      orderbook;
  } _profile;
  struct {
    core::metrics::Latency
      ping,
      heartbeat;
  } _latency;
};

}  // namespace okex
}  // namespace roq
