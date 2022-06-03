/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/core/download.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/metrics/counter.hpp"
#include "roq/core/metrics/latency.hpp"
#include "roq/core/metrics/profile.hpp"

#include "roq/core/io/context.hpp"

#include "roq/core/web/client_socket.hpp"

#include "roq/server.hpp"

#include "roq/okx/market_data_state.hpp"
#include "roq/okx/security.hpp"
#include "roq/okx/shared.hpp"

#include "roq/okx/json/parser.hpp"

namespace roq {
namespace okx {

class MarketData final : public core::web::ClientSocket::Handler, public json::Parser::Handler {
 public:
  struct SymbolsUpdate final {
    std::vector<Symbol> &symbols;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus const> const &) = 0;
    virtual void operator()(Trace<ExternalLatency const> const &) = 0;
    virtual void operator()(Trace<ReferenceData const> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketStatus const> const &, bool is_last) = 0;
    virtual void operator()(Trace<TopOfBook const> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketByPriceUpdate const> const &, bool is_last, bool refresh) = 0;
    virtual void operator()(Trace<TradeSummary const> const &, bool is_last) = 0;
    virtual void operator()(Trace<StatisticsUpdate const> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  MarketData(Handler &, core::io::Context &, uint32_t stream_id, Security &, Shared &, size_t index);

  MarketData(MarketData &&) = delete;
  MarketData(MarketData const &) = delete;

  uint16_t stream_id() const { return stream_id_; }

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &);

  void subscribe(size_t start_from = 0);

 protected:
  void operator()(core::web::ClientSocket::Connected const &) override;
  void operator()(core::web::ClientSocket::Disconnected const &) override;
  void operator()(core::web::ClientSocket::Ready const &) override;
  void operator()(core::web::ClientSocket::Close const &) override;
  void operator()(core::web::ClientSocket::Latency const &) override;
  void operator()(core::web::ClientSocket::Text const &) override;
  void operator()(core::web::ClientSocket::Binary const &) override;

 private:
  void operator()(ConnectionStatus);

  uint32_t download(MarketDataState);

  void login();

  void subscribe_static();

  void subscribe(std::span<Symbol const> const &symbols);

  void subscribe(std::string_view const &channel);
  void subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value);
  void subscribe(
      std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values);

  void parse(std::string_view const &message);

  void operator()(Trace<json::Error const> const &) override;
  void operator()(Trace<json::Subscribe const> const &) override;
  void operator()(Trace<json::Unsubscribe const> const &) override;

  void operator()(Trace<json::Status const> const &) override;
  void operator()(Trace<json::Instruments const> const &) override;
  void operator()(Trace<json::EstimatedPrice const> const &) override;
  void operator()(Trace<json::PriceLimit const> const &) override;
  void operator()(Trace<json::MarkPrice const> const &) override;
  void operator()(Trace<json::Tickers const> const &) override;
  void operator()(Trace<json::Trades const> const &) override;
  void operator()(Trace<json::BboTbt const> const &, std::string_view const &inst_id) override;
  void operator()(Trace<json::BooksL2Tbt const> const &, std::string_view const &inst_id, json::Action) override;
  void operator()(Trace<json::IndexTickers const> const &) override;
  void operator()(Trace<json::FundingRate const> const &) override;

  void operator()(Trace<json::Login const> const &) override;
  void operator()(Trace<json::Account const> const &) override;
  void operator()(Trace<json::BalanceAndPosition const> const &) override;
  void operator()(Trace<json::Positions const> const &) override;
  void operator()(Trace<json::Orders const> const &) override;
  void operator()(Trace<json::OrderAck const> const &) override;
  void operator()(Trace<json::AmendOrderAck const> const &) override;
  void operator()(Trace<json::CancelOrderAck const> const &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

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
    core::metrics::Profile parse, error, subscribe, unsubscribe, login, status, instruments, estimated_price,
        price_limit, mark_price, tickers, trades, bbo_tbt, books_l2_tbt, index_tickers, funding_rate;
  } profile_;
  struct {
    core::metrics::Latency ping, heartbeat;
  } latency_;
  // security
  Security &security_;
  // cache
  Shared &shared_;
  absl::flat_hash_set<Symbol> all_symbols_;  // only master (index 0)
  // state
  ConnectionStatus status_ = {};
  core::Download<MarketDataState> download_;
  // queue
  core::TimerQueue subscribe_queue_;
};

}  // namespace okx
}  // namespace roq
