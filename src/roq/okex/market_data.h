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

#include "roq/okex/shared.h"

#include "roq/okex/json/parser.h"

namespace roq {
namespace okex {

class MarketData final : public core::web::ClientSocket::Handler, public json::Parser::Handler {
 public:
  struct SymbolsUpdate final {
    std::vector<std::string> &symbols;
  };

  struct Handler {
    virtual void operator()(const server::Trace<StreamStatus> &) = 0;
    virtual void operator()(const server::Trace<ExternalLatency> &) = 0;
    virtual void operator()(server::Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(const server::Trace<MarketStatus> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<TopOfBook> &, bool is_last) = 0;
    virtual void operator()(
        const server::Trace<MarketByPriceUpdate> &, bool is_last, bool refresh) = 0;
    virtual void operator()(const server::Trace<TradeSummary> &, bool is_last) = 0;
    virtual void operator()(const server::Trace<StatisticsUpdate> &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  MarketData(Handler &, core::io::Context &, uint32_t stream_id, Shared &, size_t index);

  MarketData(MarketData &&) = delete;
  MarketData(const MarketData &) = delete;

  uint16_t stream_id() const { return stream_id_; }

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(const Event<Start> &);
  void operator()(const Event<Stop> &);
  void operator()(const Event<Timer> &);

  void operator()(metrics::Writer &);

  void subscribe(size_t start_from = 0);

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

  void subscribe_static();

  void subscribe(const roq::span<std::string const> &symbols);

  void subscribe(const std::string_view &channel);
  void subscribe(
      const std::string_view &channel,
      const std::string_view &selector,
      const std::string_view &value);
  void subscribe(
      const std::string_view &channel,
      const std::string_view &selector,
      const roq::span<std::string const> &values);

  void parse(const std::string_view &message);

  void operator()(server::Trace<json::Error> const &) override;
  void operator()(server::Trace<json::Subscribe> const &) override;
  void operator()(server::Trace<json::Unsubscribe> const &) override;

  void operator()(server::Trace<json::Status> const &) override;
  void operator()(server::Trace<json::Instruments> const &) override;
  void operator()(server::Trace<json::EstimatedPrice> const &) override;
  void operator()(server::Trace<json::PriceLimit> const &) override;
  void operator()(server::Trace<json::MarkPrice> const &) override;
  void operator()(server::Trace<json::Tickers> const &) override;
  void operator()(server::Trace<json::Trades> const &) override;
  void operator()(
      server::Trace<json::BooksL2Tbt> const &,
      const std::string_view &inst_id,
      json::Action) override;

  void operator()(server::Trace<json::Login> const &) override;
  void operator()(server::Trace<json::Account> const &) override;
  void operator()(server::Trace<json::BalanceAndPosition> const &) override;
  void operator()(server::Trace<json::Positions> const &) override;
  void operator()(server::Trace<json::Orders> const &) override;

 private:
  Handler &handler_;
  // config
  const uint16_t stream_id_;
  const std::string name_;
  const size_t index_;
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
    core::metrics::Profile parse, error, subscribe, unsubscribe, status, instruments,
        estimated_price, price_limit, mark_price, tickers, trades, books_l2_tbt;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // cache
  Shared &shared_;
  absl::flat_hash_set<std::string> all_symbols_;  // only master (index 0)
  // state
  ConnectionStatus status_ = {};
};

}  // namespace okex
}  // namespace roq
