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

#include "roq/okx/gateway/account.hpp"
#include "roq/okx/gateway/request.hpp"
#include "roq/okx/gateway/shared.hpp"

#include "roq/okx/protocol/json/fills_ack.hpp"
#include "roq/okx/protocol/json/orders_pending_ack.hpp"
#include "roq/okx/protocol/json/positions_ack.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct OrderEntry final : public web::rest::Client::Handler {
  struct Handler {};

  OrderEntry(Handler &, io::Context &context, uint16_t stream_id, Account &, Shared &, Request &);

  OrderEntry(OrderEntry const &) = delete;

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

 protected:
  // web::rest::Client::Handler

  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  // helpers

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  // balance

  void get_balance();
  void get_balance_ack(Trace<web::rest::Response> const &);
  // void operator()(Trace<protocol::json::BalanceAck> const &);

  // positions

  void get_positions();
  void get_positions_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::PositionsAck> const &);

  // orders-pending

  void get_orders_pending();
  void get_orders_pending_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::OrdersPendingAck> const &);

  // fills

  void get_fills();
  void get_fills_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<protocol::json::FillsAck> const &);

  // helpers

  void process_response(web::rest::Response const &, auto error_handler, auto success_handler);

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
    utils::metrics::Profile balance, balance_ack, positions, positions_ack, orders_pending, orders_pending_ack, fills, fills_ack;
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
  bool download_balance_ = false;
  bool download_positions_ = false;
  bool download_orders_ = false;
  bool download_trades_is_first_ = true;
};

}  // namespace gateway
}  // namespace okx
}  // namespace roq
