/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/server.hpp"

#include "roq/okx/account.hpp"
#include "roq/okx/drop_copy_state.hpp"
#include "roq/okx/request.hpp"
#include "roq/okx/shared.hpp"

#include "roq/okx/json/parser.hpp"
#include "roq/okx/json/trade_mode.hpp"

namespace roq {
namespace okx {

struct DropCopy final : public web::socket::Client::Handler, json::Parser::Handler {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(
        Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    virtual void operator()(Trace<PositionUpdate> const &, bool is_last) = 0;
  };

  DropCopy(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, Request &);

  DropCopy(DropCopy &&) = delete;
  DropCopy(DropCopy const &) = delete;

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
  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  void operator()(Trace<json::Error> const &) override;
  void operator()(Trace<json::Subscribe> const &) override;
  void operator()(Trace<json::Unsubscribe> const &) override;

  void operator()(Trace<json::Status> const &) override;
  void operator()(Trace<json::Instruments> const &) override;
  void operator()(Trace<json::EstimatedPrice> const &) override;
  void operator()(Trace<json::PriceLimit> const &) override;
  void operator()(Trace<json::MarkPrice> const &) override;
  void operator()(Trace<json::Tickers> const &) override;
  void operator()(Trace<json::Trades> const &) override;
  void operator()(Trace<json::BboTbt> const &, std::string_view const &inst_id) override;
  void operator()(Trace<json::BooksL2Tbt> const &, std::string_view const &inst_id, json::Action) override;
  void operator()(Trace<json::IndexTickers> const &) override;
  void operator()(Trace<json::FundingRate> const &) override;

  void operator()(Trace<json::Login> const &) override;
  void operator()(Trace<json::Account> const &) override;
  void operator()(Trace<json::BalanceAndPosition> const &) override;
  void operator()(Trace<json::Positions> const &) override;
  void operator()(Trace<json::Orders> const &) override;
  void operator()(Trace<json::OrderAck> const &) override;
  void operator()(Trace<json::AmendOrderAck> const &) override;
  void operator()(Trace<json::CancelOrderAck> const &) override;

 private:
  void operator()(ConnectionStatus);

  uint32_t download(DropCopyState);

  void login();

  void subscribe();
  void subscribe(std::string_view const &channel);
  void subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value);

  void parse(std::string_view const &message);

  void cancel_all_orders(std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_id);

  void request_balance();
  void check_response_balance();

  void request_positions();
  void check_response_positions();

  void request_orders();
  void check_response_orders();

 private:
  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::unique_ptr<web::socket::Client> const connection_;
  // buffers
  std::vector<std::byte> decode_buffer_;
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
  // account
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  ConnectionStatus status_ = {};
  core::Download<DropCopyState> download_;
  // other
  json::TradeMode const trade_mode_;
};

}  // namespace okx
}  // namespace roq
