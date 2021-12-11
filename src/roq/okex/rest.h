/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/core/buffer.h"

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/okex/rest_state.h"
#include "roq/okex/security.h"
#include "roq/okex/shared.h"

#include "roq/okex/json/instruments.h"
#include "roq/okex/json/status.h"

namespace roq {
namespace okex {

class Rest final : public core::web::Client::Handler {
 public:
  struct SymbolsUpdate final {
    std::vector<std::string> &symbols;
  };

  struct Handler {
    virtual void operator()(server::Trace<StreamStatus> const &) = 0;
    virtual void operator()(server::Trace<ExternalLatency> const &) = 0;
    virtual void operator()(server::Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(server::Trace<MarketStatus> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  Rest(Handler &, core::io::Context &context, uint16_t stream_id, Security &, Shared &);

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

  uint32_t download(RestState);

  void get_status();
  void get_status_ack(const server::Trace<core::web::Response> &, uint32_t sequence);
  void operator()(const server::Trace<json::Status> &);

  void get_instruments();
  void get_instruments_ack(const server::Trace<core::web::Response> &, uint32_t sequence);
  void operator()(const server::Trace<json::Instruments> &);

  void check_request_queue(std::chrono::nanoseconds now);

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
    core::metrics::Profile status, status_ack,  //
        instruments, instruments_ack;
  } profile_;
  struct {
    core::metrics::Latency ping;
  } latency_;
  // security
  Security &security_;
  // cache
  Shared &shared_;
  absl::flat_hash_set<std::string> all_symbols_;
  // state
  std::chrono::nanoseconds next_heartbeat_ = {};
  ConnectionStatus status_ = {};
  server::Download<RestState> download_;
};

}  // namespace okex
}  // namespace roq
