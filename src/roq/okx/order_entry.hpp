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
    virtual void operator()(const Trace<StreamStatus const> &) = 0;
    virtual void operator()(const Trace<ExternalLatency const> &) = 0;
    virtual void operator()(const Trace<TradeUpdate const> &, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(const Trace<FundsUpdate const> &, bool is_last) = 0;
    virtual void operator()(const Trace<PositionUpdate const> &, bool is_last) = 0;
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

  void operator()(Trace<json::Error const> const &) override;
  void operator()(Trace<json::Subscribe const> const &) override;
  void operator()(Trace<json::Unsubscribe const> const &) override;

  void operator()(Trace<json::Status const> const &) override;
  void operator()(Trace<json::Instruments const> const &) override;
  void operator()(Trace<json::EstimatedPrice const> const &) override;
  void operator()(Trace<json::PriceLimit const> const &) override;
  void operator()(Trace<json::MarkPrice const> const &) override;
  void operator()(Trace<json::Tickers const> const &) override;
  void operator()(Trace<json::Trades const> const &) override;
  void operator()(
      Trace<json::BooksL2Tbt const> const &,
      const std::string_view &inst_id,
      json::Action) override;
  void operator()(Trace<json::IndexTickers const> const &) override;
  void operator()(Trace<json::FundingRate const> const &) override;

  void operator()(Trace<json::Login const> const &) override;
  void operator()(Trace<json::Account const> const &) override;
  void operator()(Trace<json::BalanceAndPosition const> const &) override;
  void operator()(Trace<json::Positions const> const &) override;
  void operator()(Trace<json::Orders const> const &) override;
  void operator()(Trace<json::OrderAck const> const &) override;
  void operator()(Trace<json::AmendOrderAck const> const &) override;
  void operator()(Trace<json::CancelOrderAck const> const &) override;

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
