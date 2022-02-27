/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/buffer.h"
#include "roq/core/download.h"

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client.h"

#include "roq/server.h"

#include "roq/okx/request.h"
#include "roq/okx/security.h"
#include "roq/okx/shared.h"

#include "roq/okx/json/orders.h"

namespace roq {
namespace okx {

class Rest final : public core::web::Client::Handler {
 public:
  struct Handler {
    virtual void operator()(server::Trace<StreamStatus> const &) = 0;
    virtual void operator()(server::Trace<ExternalLatency> const &) = 0;
  };

  Rest(Handler &, core::io::Context &context, uint16_t stream_id, Security &, Shared &, Request &);

  Rest(Rest &&) = delete;
  Rest(const Rest &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

 protected:
  void operator()(const core::web::Client::Connected &) override;
  void operator()(const core::web::Client::Disconnected &) override;
  void operator()(const core::web::Client::Latency &) override;

  void operator()(ConnectionStatus);

  void get_orders();
  void get_orders_ack(const server::Trace<core::web::Response> &);
  void operator()(const server::Trace<json::Orders> &);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  core::web::Client connection_;
  // buffers
  core::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile orders, orders_ack;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
  // security
  Security &security_;
  // shared
  Shared &shared_;
  Request &request_;
  // state
  ConnectionStatus status_ = {};
  bool download_orders_ = false;
};

}  // namespace okx
}  // namespace roq
