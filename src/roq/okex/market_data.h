/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/core/metrics/counter.h"
#include "roq/core/metrics/latency.h"
#include "roq/core/metrics/profile.h"

#include "roq/core/io/context.h"

#include "roq/core/web/client_socket.h"

#include "roq/download.h"
#include "roq/server.h"

#include "roq/okex/market_data_state.h"
#include "roq/okex/shared.h"

#include "roq/okex/json/parser.h"

namespace roq {
namespace okex {

class MarketData final : public core::web::ClientSocket::Handler, public json::Parser::Handler {
 public:
  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(const server::Trace<MarketStatus> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<TopOfBook> &, bool is_last) = 0;
    virtual void operator()(
        const server::Trace<MarketByPriceUpdate> &, bool is_last, bool refresh) = 0;
    virtual void operator()(const server::Trace<TradeSummary> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<StatisticsUpdate> &, bool is_last) = 0;
  };

  MarketData(
      Handler &,
      core::io::Context &,
      uint32_t stream_id,
      Shared &,
      const std::string_view &uri,
      const std::string_view &query,
      std::chrono::nanoseconds ping_frequency);

  MarketData(MarketData &&) = delete;
  MarketData(const MarketData &) = delete;

  uint16_t stream_id() const { return stream_id_; }

  bool ready() const { return ready_; }

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

  void update_subscriptions(std::vector<std::string> &symbols);

 protected:
  void operator()(const core::web::ClientSocket::Connected &) override;
  void operator()(const core::web::ClientSocket::Disconnected &) override;
  void operator()(const core::web::ClientSocket::Ready &) override;
  void operator()(const core::web::ClientSocket::Close &) override;
  void operator()(const core::web::ClientSocket::Latency &) override;
  void operator()(const core::web::ClientSocket::Text &) override;
  void operator()(const core::web::ClientSocket::Binary &) override;

 private:
  void operator()(ConnectionStatus);

  uint32_t download(MarketDataState);

  void subscribe(const roq::span<std::string> &symbols);

  void subscribe(const std::string_view &topic, const roq::span<std::string> &symbols);

  void send_ping(std::chrono::nanoseconds now);

  void parse(const std::string_view &message);

  void operator()(server::Trace<json::Welcome> const &) override;
  void operator()(server::Trace<json::Error> const &) override;
  void operator()(server::Trace<json::Pong> const &) override;
  void operator()(server::Trace<json::Ack> const &) override;

  void operator()(server::Trace<json::Snapshot> const &) override;
  void operator()(server::Trace<json::Ticker> const &) override;
  void operator()(server::Trace<json::Level2> const &) override;

  void operator()(server::Trace<json::AccountBalance> const &) override;
  void operator()(server::Trace<json::OrderChange> const &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  const std::chrono::nanoseconds ping_frequency_;
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
    core::metrics::Profile parse, welcome, error, pong, ack, snapshot, ticker, level2;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // cache
  Shared &shared_;
  std::vector<std::string> symbols_;
  // state
  bool welcome_ = false;
  bool ready_ = false;
  ConnectionStatus status_ = {};
  server::Download<MarketDataState> download_;
  std::chrono::nanoseconds logon_timeout_ = {};
  std::chrono::nanoseconds next_ping_ = {};
  // queue
  std::deque<std::pair<std::chrono::nanoseconds, std::string> > subscribe_queue_;
};

}  // namespace okex
}  // namespace roq
