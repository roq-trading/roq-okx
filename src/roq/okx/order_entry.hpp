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
    virtual void operator()(Trace<StreamStatus const> const &) = 0;
    virtual void operator()(Trace<ExternalLatency const> const &) = 0;
    virtual void operator()(Trace<TradeUpdate const> const &, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(Trace<FundsUpdate const> const &, bool is_last) = 0;
    virtual void operator()(Trace<PositionUpdate const> const &, bool is_last) = 0;
  };

  OrderEntry(Handler &, core::io::Context &, uint16_t stream_id, Security &, Shared &, Request &);

  OrderEntry(OrderEntry &&) = delete;
  OrderEntry(OrderEntry const &) = delete;

  bool ready() const;

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  uint16_t operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id);
  uint16_t operator()(
      Event<ModifyOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  uint16_t operator()(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);

  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id);

 protected:
  void operator()(core::web::ClientSocket::Connected const &) override;
  void operator()(core::web::ClientSocket::Disconnected const &) override;
  void operator()(core::web::ClientSocket::Ready const &) override;
  void operator()(core::web::ClientSocket::Close const &) override;
  void operator()(core::web::ClientSocket::Latency const &) override;
  void operator()(core::web::ClientSocket::Text const &) override;
  void operator()(core::web::ClientSocket::Binary const &) override;

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
  void operator()(Trace<json::BooksL2Tbt const> const &, std::string_view const &inst_id, json::Action) override;
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
  void subscribe(std::string_view const &channel);
  void subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value);

  void parse(std::string_view const &message);

  void cancel_all_orders(std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_id);

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
    core::metrics::Profile parse, error, subscribe, unsubscribe, login, account, balance_and_position, positions,
        orders, create_order, modify_order, cancel_order, order_ack, amend_order_ack, cancel_order_ack;
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
