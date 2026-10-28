/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/server/stream.hpp"

#include "roq/okx/gateway/account.hpp"
#include "roq/okx/gateway/request.hpp"
#include "roq/okx/gateway/shared.hpp"

#include "roq/okx/protocol/json/account_balance_ack.hpp"
#include "roq/okx/protocol/json/account_config_ack.hpp"
#include "roq/okx/protocol/json/account_positions_ack.hpp"
#include "roq/okx/protocol/json/trade_fills_ack.hpp"
#include "roq/okx/protocol/json/trade_orders_pending_ack.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct OrderEntry final : public Base<OrderEntry>, public server::Stream, public web::rest::Client::Handler {
  struct Handler {};

  OrderEntry(Handler &, io::Context &context, uint16_t stream_id, Account &, Shared &, Request &);

  // protected:
  friend base_type;

  // server::Stream

  uint16_t stream_id() const override { return stream_id_; }

  bool ready() const override { return connection_status_ == ConnectionStatus::READY; }

  void operator()(Trace<Start> const &) override;
  void operator()(Trace<Stop> const &) override;
  void operator()(Trace<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  void operator()(Trace<ConnectionStatus> const &, std::string_view const &reason = {}) override;

 protected:
  // web::rest::Client::Handler

  void operator()(Trace<web::rest::Connected> const &) override;
  void operator()(Trace<web::rest::Disconnected> const &) override;
  void operator()(Trace<web::rest::Latency> const &) override;

  // account-config

  void get_account_config(TraceInfo const &);
  void get_account_config_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::AccountConfigAck> const &);

  // account-balance

  void get_account_balance(TraceInfo const &);
  void get_account_balance_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::AccountBalanceAck> const &);

  // account-positions

  void get_account_positions(TraceInfo const &);
  void get_account_positions_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::AccountPositionsAck> const &);

  // trade-orders-pending

  void get_trade_orders_pending(TraceInfo const &);
  void get_trade_orders_pending_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::TradeOrdersPendingAck> const &);

  // trade-fills

  void get_trade_fills(TraceInfo const &);
  void get_trade_fills_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::TradeFillsAck> const &);

  // helpers

  void check_request_queue(TraceInfo const &);

  void process_response(Trace<web::rest::Response> const &, auto error_handler, auto success_handler);

 private:
  [[maybe_unused]] Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // connection
  std::unique_ptr<web::rest::Client> const connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile account_config, account_config_ack, account_balance, account_balance_ack, account_positions, account_positions_ack,
        trade_orders_pending, trade_orders_pending_ack, trade_fills, trade_fills_ack;
  } profile_;
  struct {
    utils::metrics::Latency ping;
  } latency_;
  // account
  Account &account_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  ConnectionStatus connection_status_ = {};
  bool download_trades_is_first_ = true;
};

}  // namespace gateway
}  // namespace okx
}  // namespace roq
