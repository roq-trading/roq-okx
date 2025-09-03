/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/okx/account.hpp"
#include "roq/okx/request.hpp"
#include "roq/okx/shared.hpp"

// #include "roq/okx/json/balance.hpp"
#include "roq/okx/json/fills.hpp"
#include "roq/okx/json/orders.hpp"
#include "roq/okx/json/positions_rest.hpp"

namespace roq {
namespace okx {

struct OrderEntry final : public web::rest::Client::Handler {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<TradeUpdate> const &, bool is_last, uint8_t user_id, std::string_view const &request_id) = 0;
    virtual void operator()(Trace<FundsUpdate> const &, bool is_last) = 0;
    virtual void operator()(Trace<PositionUpdate> const &, bool is_last) = 0;
  };

  OrderEntry(Handler &, io::Context &context, uint16_t stream_id, Account &, Shared &, Request &);

  OrderEntry(OrderEntry const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

 protected:
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  void operator()(ConnectionStatus);

  void get_balance();
  void get_balance_ack(Trace<web::rest::Response> const &);
  // void operator()(Trace<json::Balance> const &);

  void get_positions();
  void get_positions_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::PositionsRest> const &);

  void get_orders();
  void get_orders_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::Orders> const &);

  void get_fills();
  void get_fills_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::Fills> const &);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

 private:
  Handler &handler_;
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
    utils::metrics::Profile balance, balance_ack, positions, positions_ack, orders, orders_ack, fills, fills_ack;
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
  ConnectionStatus status_ = {};
  bool download_balance_ = false;
  bool download_positions_ = false;
  bool download_orders_ = false;
  bool download_trades_is_first_ = true;
};

}  // namespace okx
}  // namespace roq
