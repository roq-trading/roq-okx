/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/web_socket.h"

#include <fmt/format.h>

#include "roq/core/clock.h"

#include "roq/okex/flags.h"
#include "roq/okex/gateway.h"

#include "roq/okex/json/error.h"
#include "roq/okex/json/method.h"
#include "roq/okex/json/request_type.h"

namespace roq {
namespace okex {

namespace {
constexpr std::string_view CONNECTION = "ws";

static auto create_counter(const std::string_view &function) {
  return core::metrics::Counter(Flags::name(), CONNECTION, function);
}

static auto create_profile(const std::string_view &function) {
  return core::metrics::Profile(Flags::name(), CONNECTION, function);
}

static auto create_latency(const std::string_view &function) {
  return core::metrics::Latency(Flags::name(), CONNECTION, function);
}
}  // namespace

WebSocket::WebSocket(
    Gateway &gateway,
    const Config &config,
    Random &random,
    core::event::Base &base,
    core::event::DNSBase &dns_base,
    core::ssl::Context &ssl_context)
    : _gateway(gateway), _access_key(config.get_access_key()), _random(random),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(Flags::ws_uri()),
          std::string_view(),  // query
          std::chrono::seconds{Flags::ping_freq_secs()},
          Flags::decode_buffer_size(),  // XXX need read buffer size
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      _decode_buffer(Flags::decode_buffer_size()),
      _counter{
          .disconnect = create_counter("disconnect"),
      },
      _profile{
          .parse = create_profile("parse"),
          .get_symbols = create_profile("get_symbols"),
          .get_trading_balance = create_profile("get_trading_balance"),
          .get_orders = create_profile("get_symbols"),
          .order = create_profile("order"),
          .ticker = create_profile("ticker"),
          .trades = create_profile("trades"),
          .orderbook = create_profile("orderbook"),
      },
      _latency{
          .ping = create_latency("ping"),
          .heartbeat = create_latency("heartbeat"),
      } {
}

bool WebSocket::ready() const {
  return _connection.ready();
}

void WebSocket::close() {
  _connection.close();
}

void WebSocket::operator()(const Event<Start> &) {
  _connection.start();
}

void WebSocket::operator()(const Event<Stop> &) {
  _connection.stop();
}

void WebSocket::operator()(const Event<Timer> &event) {
  _connection.refresh(event.value.now);
}

void WebSocket::login() {
  LOG(INFO)("Sending login request...");
  constexpr json::RequestType request_type = json::RequestType::LOGIN;
  auto nonce = _random.create_nonce();
  auto signature = _random.create_signature(nonce);
  auto message = fmt::format(
      R"({{)"
      R"("method":"login",)"
      R"("params":{{)"
      R"("algo":"HS256",)"
      R"("pKey":"{}",)"
      R"("nonce":"{}",)"
      R"("signature":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      _access_key,
      nonce,
      signature,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::get_symbols() {
  constexpr json::RequestType request_type = json::RequestType::GET_SYMBOLS;
  auto message = fmt::format(
      R"({{)"
      R"("method":"getSymbols",)"
      R"("params":{{}},)"
      R"("id":"{}")"
      R"(}})",
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::get_trading_balance() {
  constexpr json::RequestType request_type =
      json::RequestType::GET_TRADING_BALANCE;
  auto message = fmt::format(
      R"({{)"
      R"("method":"getTradingBalance",)"
      R"("params":{{}},)"
      R"("id":"{}")"
      R"(}})",
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::get_orders() {
  constexpr json::RequestType request_type = json::RequestType::GET_ORDERS;
  auto message = fmt::format(
      R"({{)"
      R"("method":"getOrders",)"
      R"("params":{{}},)"
      R"("id":"{}")"
      R"(}})",
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::new_order(
    const CreateOrder &create_order, const std::string_view &request_id) {
  constexpr json::RequestType request_type = json::RequestType::NEW_ORDER;
  auto message = fmt::format(
      R"({{)"
      R"("method":"newOrder",)"
      R"("params":{{")"
      R"("clientOrderId":"{}",)"
      R"("symbol":"{}",)"
      R"("side":"{}",)"
      R"("type":"{}",)"
      R"("timeInForce":"{}",)"
      R"("quantity":"{}",)"
      R"("price":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      request_id,
      create_order.symbol,
      "sell",   // XXX
      "limit",  // XXX
      "GTC",    // XXX
      create_order.quantity,
      create_order.price,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::cancel_replace_order(
    const ModifyOrder &modify_order,
    const std::string_view &request_id,
    const server::OMS_Order &order) {
  constexpr json::RequestType request_type =
      json::RequestType::CANCEL_REPLACE_ORDER;
  auto message = fmt::format(
      R"({{)"
      R"("method":"cancelOrder",)"
      R"("params":{{)"
      R"("clientOrderId":"{}",)"
      R"("requestClientId":"{}",)"
      R"("quantity":"{}",)"
      R"("price":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}}")",
      order.external_order_id,  // XXX WRONG !!!
      request_id,
      modify_order.quantity,
      modify_order.price,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::cancel_order(const server::OMS_Order &order) {
  constexpr json::RequestType request_type = json::RequestType::CANCEL_ORDER;
  auto message = fmt::format(
      R"({{)"
      R"("method":"cancelOrder",)"
      R"("params":{{)"
      R"("clientOrderId":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      order.external_order_id,  // XXX WRONG !!!
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::subscribe_ticker(const std::string_view &symbol) {
  constexpr json::RequestType request_type =
      json::RequestType::SUBSCRIBE_TICKER;
  auto message = fmt::format(
      R"({{)"
      R"("method":"subscribeTicker",)"
      R"("params":{{)"
      R"("symbol":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      symbol,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::subscribe_trades(const std::string_view &symbol) {
  constexpr json::RequestType request_type =
      json::RequestType::SUBSCRIBE_TRADES;
  auto message = fmt::format(
      R"({{)"
      R"("method":"subscribeTrades",)"
      R"("params":{{)"
      R"("symbol":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      symbol,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::subscribe_orderbook(const std::string_view &symbol) {
  constexpr json::RequestType request_type =
      json::RequestType::SUBSCRIBE_ORDERBOOK;
  auto message = fmt::format(
      R"({{)"
      R"("method":"subscribeOrderbook",)"
      R"("params":{{)"
      R"("symbol":"{}")"
      R"(}},)"
      R"("id":"{}")"
      R"(}})",
      symbol,
      request_type.as_raw_text());
  _connection.send_text(message);
}

void WebSocket::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(_counter.disconnect, metrics::COUNTER)
      // profile
      .write(_profile.parse, metrics::PROFILE)
      .write(_profile.get_symbols, metrics::PROFILE)
      .write(_profile.get_trading_balance, metrics::PROFILE)
      .write(_profile.get_orders, metrics::PROFILE)
      .write(_profile.order, metrics::PROFILE)
      .write(_profile.ticker, metrics::PROFILE)
      .write(_profile.trades, metrics::PROFILE)
      .write(_profile.orderbook, metrics::PROFILE)
      // latency
      .write(_latency.ping, metrics::LATENCY)
      .write(_latency.heartbeat, metrics::LATENCY);
}

void WebSocket::operator()(const core::web::Socket::Connected &) {
  // note! wait for upgrade
}

void WebSocket::operator()(const core::web::Socket::Disconnected &) {
  ++_counter.disconnect;
  _gateway(*this);
}

void WebSocket::operator()(const core::web::Socket::Ready &) {
  login();
}

void WebSocket::operator()(const core::web::Socket::Close &) {
}

void WebSocket::operator()(const core::web::Socket::Latency &latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(latency.sample)
          .count());
}

void WebSocket::operator()(const core::web::Socket::Text &text) {
  parse(text.payload);
}

void WebSocket::parse(const std::string_view &message) {
  _profile.parse([&]() {
    try {
      core::jsonrpc::Parser::dispatch(*this, message);
    } catch (std::exception &e) {
      LOG(WARNING)(R"(message="{}")", message);
      LOG(FATAL)(R"("ERROR what="{}")", e.what());
    }
  });
}

void WebSocket::operator()(
    const core::jsonrpc::Error &error, core::json::value_t &value) {
  json::Error error_2(value);
  LOG(WARNING)(R"(error={}, id="{}")", error_2, error.id);
  switch (error_2.code) {
    case 1:  // invalid json
      _connection.close();
      break;
    default:
      LOG(FATAL)("Unexpected");
  }
}

void WebSocket::operator()(
    const core::jsonrpc::Result &result, core::json::value_t &value) {
  json::RequestType request_type(result.id);
  switch (request_type) {
    case json::RequestType::UNDEFINED:
      break;
    case json::RequestType::UNKNOWN:
      DLOG(FATAL)(R"("Unknown request_type="{}")", result.id);
      break;
    case json::RequestType::LOGIN: {
      LOG(INFO)("Ready");
      _gateway(*this);
      break;
    }
    case json::RequestType::SUBSCRIBE_TICKER: {
      break;
    }
    case json::RequestType::SUBSCRIBE_TRADES: {
      break;
    }
    case json::RequestType::SUBSCRIBE_ORDERBOOK: {
      break;
    }
    case json::RequestType::GET_SYMBOLS: {
      core::json::Buffer buffer(_decode_buffer);
      json::Symbols symbols(value, buffer);
      (*this)(symbols);
      break;
    }
    case json::RequestType::GET_TRADING_BALANCE: {
      core::json::Buffer buffer(_decode_buffer);
      json::TradingBalance trading_balance(value, buffer);
      (*this)(trading_balance);
      break;
    }
    case json::RequestType::GET_ORDERS: {
      core::json::Buffer buffer(_decode_buffer);
      json::Orders orders(value, buffer);
      (*this)(orders);
      break;
    }
    case json::RequestType::NEW_ORDER: {
      json::Order order(value);
      (*this)(order);
      break;
    }
    case json::RequestType::CANCEL_ORDER: {
      json::Order order(value);
      (*this)(order);
      break;
    }
    case json::RequestType::CANCEL_REPLACE_ORDER: {
      json::Order order(value);
      (*this)(order);
      break;
    }
  }
}

void WebSocket::operator()(
    const core::jsonrpc::Notification &notification,
    core::json::value_t &value) {
  json::Method method(notification.method);
  switch (method) {
    case json::Method::UNDEFINED:
      break;
    case json::Method::UNKNOWN:
      DLOG(FATAL)(R"(Unknown method="{}")", notification.method);
      break;
    case json::Method::TICKER: {
      json::Ticker ticker(value);
      (*this)(ticker);
      break;
    }
    case json::Method::SNAPSHOT_ORDERBOOK: {
      core::json::Buffer buffer(_decode_buffer);
      json::Orderbook orderbook(value, buffer);
      (*this)(orderbook, true);
      break;
    }
    case json::Method::UPDATE_ORDERBOOK: {
      core::json::Buffer buffer(_decode_buffer);
      json::Orderbook orderbook(value, buffer);
      (*this)(orderbook, false);
      break;
    }
    case json::Method::SNAPSHOT_TRADES: {
      core::json::Buffer buffer(_decode_buffer);
      json::Trades trades(value, buffer);
      (*this)(trades, true);
      break;
    }
    case json::Method::UPDATE_TRADES: {
      core::json::Buffer buffer(_decode_buffer);
      json::Trades trades(value, buffer);
      (*this)(trades, false);
      break;
    }
  }
}

void WebSocket::operator()(const json::Symbols &symbols) {
  _profile.get_symbols([&]() {
    VLOG(1)(R"(symbols={})", symbols);
    _gateway(symbols);
  });
}

void WebSocket::operator()(const json::TradingBalance &trading_balance) {
  _profile.get_trading_balance([&]() {
    VLOG(1)(R"(trading_balance={})", trading_balance);
    _gateway(trading_balance);
  });
}

void WebSocket::operator()(const json::Orders &orders) {
  _profile.get_orders([&]() {
    VLOG(1)(R"(orders={})", orders);
    _gateway(orders);
  });
}

void WebSocket::operator()(const json::Order &order) {
  _profile.order([&]() {
    VLOG(1)(R"(order={})", order);
    _gateway(order);
  });
}

void WebSocket::operator()(const json::Ticker &ticker) {
  _profile.ticker([&]() {
    VLOG(3)(R"(ticker={})", ticker);
    _gateway(ticker);
  });
}

void WebSocket::operator()(const json::Trades &trades, bool snapshot) {
  _profile.trades([&]() {
    VLOG(3)(R"(trades={}, snapshot={})", trades, snapshot);
    if (snapshot == false)
      _gateway(trades);
  });
}

void WebSocket::operator()(const json::Orderbook &orderbook, bool snapshot) {
  _profile.orderbook([&]() {
    VLOG(3)(R"(orderbook={}, snapshot={})", orderbook, snapshot);
    _gateway(orderbook, snapshot);
  });
}

}  // namespace okex
}  // namespace roq
