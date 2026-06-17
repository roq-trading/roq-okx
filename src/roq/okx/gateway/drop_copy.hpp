/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/okx/gateway/account.hpp"
#include "roq/okx/gateway/request.hpp"
#include "roq/okx/gateway/shared.hpp"

#include "roq/okx/protocol/json/parser.hpp"
#include "roq/okx/protocol/json/trade_mode.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct DropCopy final : public web::socket::Client::Handler, protocol::json::Parser::Handler {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<TradeUpdate> const &, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    virtual void operator()(Trace<PositionUpdate> const &, bool is_last) = 0;
  };

  DropCopy(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &);

  DropCopy(DropCopy const &) = delete;

  bool ready() const;

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

  uint16_t operator()(Event<CreateOrder> const &, server::oms::Order const &, server::oms::RefData const &, std::string_view const &request_id);
  uint16_t operator()(
      Event<ModifyOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);
  uint16_t operator()(
      Event<CancelOrder> const &,
      server::oms::Order const &,
      server::oms::RefData const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id);

  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id);

 protected:
  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  // protocol::json::Parser::Handler

  void operator()(Trace<protocol::json::Error> const &) override;
  void operator()(Trace<protocol::json::Subscribe> const &) override;
  void operator()(Trace<protocol::json::Unsubscribe> const &) override;

  void operator()(Trace<protocol::json::Status> const &) override;
  void operator()(Trace<protocol::json::Instruments> const &) override;
  void operator()(Trace<protocol::json::EstimatedPrice> const &) override;
  void operator()(Trace<protocol::json::PriceLimit> const &) override;
  void operator()(Trace<protocol::json::MarkPrice> const &) override;
  void operator()(Trace<protocol::json::Tickers> const &) override;
  void operator()(Trace<protocol::json::Trades> const &) override;
  void operator()(Trace<protocol::json::BboTbt> const &) override;
  void operator()(Trace<protocol::json::BooksL2Tbt> const &) override;
  void operator()(Trace<protocol::json::IndexTickers> const &) override;
  void operator()(Trace<protocol::json::FundingRate> const &) override;

  void operator()(Trace<protocol::json::ChannelConnCount> const &) override;
  void operator()(Trace<protocol::json::Login> const &) override;
  void operator()(Trace<protocol::json::Account> const &) override;
  void operator()(Trace<protocol::json::BalanceAndPosition> const &) override;
  void operator()(Trace<protocol::json::Positions> const &) override;
  void operator()(Trace<protocol::json::Orders> const &) override;
  void operator()(Trace<protocol::json::Order> const &) override;
  void operator()(Trace<protocol::json::AmendOrder> const &) override;
  void operator()(Trace<protocol::json::CancelOrder> const &) override;

  void operator()(Trace<protocol::json::Candle> const &) override;

 private:
  // helpers

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    LOGIN,
    SUBSCRIBE,
    BALANCE,
    POSITIONS,
    ORDERS,
    DONE,
  };

  uint32_t download(State);

  void login();

  void subscribe();
  void subscribe(std::string_view const &channel);
  void subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value);

  void parse(std::string_view const &message);

  void request_balance();
  void request_positions();
  void request_orders();

  void check_response_balance();
  void check_response_positions();
  void check_response_orders();

  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::unique_ptr<web::socket::Client> const connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  std::string encode_buffer_;
  // session
  uint64_t request_id_ = {};
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse, error, subscribe, unsubscribe, channel_conn_count, login, account, balance_and_position, positions, orders, create_order,
        modify_order, cancel_order, order_ack, amend_order_ack, cancel_order_ack;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // account
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  ConnectionStatus connection_status_ = {};
  core::Download<State> download_;
  // other
  protocol::json::TradeMode const trade_mode_;
};

}  // namespace gateway
}  // namespace okx
}  // namespace roq
