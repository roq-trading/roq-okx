/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/buffer.hpp"
#include "roq/core/download.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/server.hpp"

#include "roq/okx/request.hpp"
#include "roq/okx/security.hpp"
#include "roq/okx/shared.hpp"

#include "roq/okx/json/orders.hpp"

namespace roq {
namespace okx {

struct Rest final : public web::rest::Client::Handler {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
  };

  Rest(Handler &, io::Context &context, uint16_t stream_id, Security &, Shared &, Request &);

  Rest(Rest &&) = delete;
  Rest(Rest const &) = delete;

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

 protected:
  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;
  void operator()(Trace<web::rest::Response> const &, uint64_t request_id, uint64_t opaque) override;

  void operator()(ConnectionStatus);

  void get_orders();
  void get_orders_ack(Trace<web::rest::Response> const &);
  void operator()(Trace<json::Orders> const &);

  template <typename SuccessHandler, typename ErrorHandler>
  void process_response(web::rest::Response const &, SuccessHandler, ErrorHandler);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // connection
  std::unique_ptr<web::rest::Client> connection_;
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
