/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client_socket.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/okex/drop_copy_state.h"
#include "roq/okex/security.h"
#include "roq/okex/shared.h"

#include "roq/okex/json/parser.h"

namespace roq {
namespace okex {

class DropCopy final : public core::web::ClientSocket::Handler, json::Parser::Handler {
 public:
  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(const server::Trace<TradeUpdate> &, bool is_last, uint8_t user_id) = 0;
    virtual void operator()(const server::Trace<FundsUpdate> &, bool is_last) = 0;
  };

  DropCopy(
      Handler &,
      core::io::Context &,
      uint16_t stream_id,
      Security &,
      Shared &,
      const std::string_view &uri,
      const std::string_view &query,
      std::chrono::nanoseconds ping_frequency);

  DropCopy(DropCopy &&) = delete;
  DropCopy(const DropCopy &) = delete;

  bool ready() const;

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

 protected:
  void operator()(const core::web::ClientSocket::Connected &) override;
  void operator()(const core::web::ClientSocket::Disconnected &) override;
  void operator()(const core::web::ClientSocket::Ready &) override;
  void operator()(const core::web::ClientSocket::Close &) override;
  void operator()(const core::web::ClientSocket::Latency &) override;
  void operator()(const core::web::ClientSocket::Text &) override;
  void operator()(const core::web::ClientSocket::Binary &) override;

  void operator()(server::Trace<json::Welcome> const &) override;
  void operator()(server::Trace<json::Error> const &) override;
  void operator()(server::Trace<json::Pong> const &) override;
  void operator()(server::Trace<json::Ack> const &) override;

  void operator()(server::Trace<json::Snapshot> const &) override;
  void operator()(server::Trace<json::Ticker> const &) override;
  void operator()(server::Trace<json::Level2> const &) override;

  void operator()(server::Trace<json::AccountBalance> const &) override;
  void operator()(server::Trace<json::OrderChange> const &) override;

 private:
  void operator()(ConnectionStatus);

  uint32_t download(DropCopyState);

  void subscribe();

  void subscribe(const std::string_view &topic);

  void parse(const std::string_view &message);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  // web socket
  core::web::ClientSocket connection_;
  const std::chrono::nanoseconds ping_frequency_;
  // buffers
  core::Buffer decode_buffer_;
  // metrics
  struct {
    core::metrics::Counter disconnect;
  } counter_;
  struct {
    core::metrics::Profile parse, welcome, error, pong, ack, account_balance, order_change;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // security
  Security &security_;
  // cache
  Shared &shared_;
  // state
  bool welcome_ = false;
  bool ready_ = false;
  ConnectionStatus status_ = {};
  server::Download<DropCopyState> download_;
  std::chrono::nanoseconds logon_timeout_ = {};
  std::chrono::nanoseconds next_ping_ = {};
};

}  // namespace okex
}  // namespace roq
