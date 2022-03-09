/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/core/io/context.hpp"

#include "roq/core/web/client_socket.hpp"

#include "roq/server.hpp"

#include "roq/okx/order_entry_state.hpp"
#include "roq/okx/request.hpp"
#include "roq/okx/security.hpp"
#include "roq/okx/shared.hpp"

#include "roq/okx/json/parser.hpp"
#include "roq/okx/json/trade_mode.hpp"

namespace roq {
namespace okx {

class OrderEntry final : public core::web::ClientSocket::Handler, json::Parser::Handler {
 public:
  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(const server::Trace<TradeUpdate> &, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(const server::Trace<FundsUpdate> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<PositionUpdate> &, bool is_last) = 0;
  };

  OrderEntry(Handler &, core::io::Context &, uint16_t stream_id, Security &, Shared &, Request &);

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(const OrderEntry &) = delete;

  bool ready() const;

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

  uint16_t operator()(
      const Event<CreateOrder> &, const oms::Order &, const std::string_view &request_id);
  uint16_t operator()(
      const Event<ModifyOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id);
  uint16_t operator()(
      const Event<CancelOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id);

  uint16_t operator()(const Event<CancelAllOrders> &, const std::string_view &request_id);

 protected:
  void operator()(const core::web::ClientSocket::Connected &) override;
  void operator()(const core::web::ClientSocket::Disconnected &) override;
  void operator()(const core::web::ClientSocket::Ready &) override;
  void operator()(const core::web::ClientSocket::Close &) override;
  void operator()(const core::web::ClientSocket::Latency &) override;
  void operator()(const core::web::ClientSocket::Text &) override;
  void operator()(const core::web::ClientSocket::Binary &) override;

  void operator()(server::Trace<json::Error> const &) override;
  void operator()(server::Trace<json::Subscribe> const &) override;
  void operator()(server::Trace<json::Unsubscribe> const &) override;

  void operator()(server::Trace<json::Status> const &) override;
  void operator()(server::Trace<json::Instruments> const &) override;
  void operator()(server::Trace<json::EstimatedPrice> const &) override;
  void operator()(server::Trace<json::PriceLimit> const &) override;
  void operator()(server::Trace<json::MarkPrice> const &) override;
  void operator()(server::Trace<json::Tickers> const &) override;
  void operator()(server::Trace<json::Trades> const &) override;
  void operator()(
      server::Trace<json::BooksL2Tbt> const &,
      const std::string_view &inst_id,
      json::Action) override;
  void operator()(server::Trace<json::IndexTickers> const &) override;
  void operator()(server::Trace<json::FundingRate> const &) override;

  void operator()(server::Trace<json::Login> const &) override;
  void operator()(server::Trace<json::Account> const &) override;
  void operator()(server::Trace<json::BalanceAndPosition> const &) override;
  void operator()(server::Trace<json::Positions> const &) override;
  void operator()(server::Trace<json::Orders> const &) override;
  void operator()(server::Trace<json::OrderAck> const &) override;
  void operator()(server::Trace<json::AmendOrderAck> const &) override;
  void operator()(server::Trace<json::CancelOrderAck> const &) override;

 private:
  void operator()(ConnectionStatus);

  uint32_t download(OrderEntryState);

  void login();

  void subscribe();
  void subscribe(const std::string_view &channel);
  void subscribe(
      const std::string_view &channel,
      const std::string_view &selector,
      const std::string_view &value);

  void parse(const std::string_view &message);

  void cancel_all_orders(
      const std::span<std::pair<std::string_view, std::string_view>> &symbol_and_external_order_id);

  void request_orders();
  void check_response_orders();

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // web socket
  core::web::ClientSocket connection_;
  // buffers
  core::Buffer decode_buffer_;
  // session
  uint64_t request_id_ = {};
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile parse, error, subscribe, unsubscribe, login, account,
        balance_and_position, positions, orders, create_order, modify_order, cancel_order,
        order_ack, amend_order_ack, cancel_order_ack;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // security
  Security &security_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  ConnectionStatus status_ = {};
  core::Download<OrderEntryState> download_;
  // other
  const json::TradeMode trade_mode_;
};

}  // namespace okx
}  // namespace roq
