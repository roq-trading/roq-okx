/* Copyright (c) 2017-2023, Hans Erik Thrane */

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

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/server.hpp"

#include "roq/okx/account.hpp"
#include "roq/okx/market_data_state.hpp"
#include "roq/okx/shared.hpp"

#include "roq/okx/json/parser.hpp"

namespace roq {
namespace okx {

struct MarketData final : public web::socket::Client::Handler, public json::Parser::Handler {
  struct SymbolsUpdate final {
    std::vector<Symbol> &symbols;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketStatus> const &, bool is_last) = 0;
    virtual void operator()(Trace<TopOfBook> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketByPriceUpdate> const &, bool is_last) = 0;
    virtual void operator()(Trace<TradeSummary> const &, bool is_last) = 0;
    virtual void operator()(Trace<StatisticsUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  MarketData(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &, size_t index);

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
  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

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

  void operator()(Trace<json::Error> const &) override;
  void operator()(Trace<json::Subscribe> const &) override;
  void operator()(Trace<json::Unsubscribe> const &) override;

  void operator()(Trace<json::Status> const &) override;
  void operator()(Trace<json::Instruments> const &) override;
  void operator()(Trace<json::EstimatedPrice> const &) override;
  void operator()(Trace<json::PriceLimit> const &) override;
  void operator()(Trace<json::MarkPrice> const &) override;
  void operator()(Trace<json::Tickers> const &) override;
  void operator()(Trace<json::Trades> const &) override;
  void operator()(Trace<json::BboTbt> const &, std::string_view const &inst_id) override;
  void operator()(Trace<json::BooksL2Tbt> const &, std::string_view const &inst_id, json::Action) override;
  void operator()(Trace<json::IndexTickers> const &) override;
  void operator()(Trace<json::FundingRate> const &) override;

  void operator()(Trace<json::Login> const &) override;
  void operator()(Trace<json::Account> const &) override;
  void operator()(Trace<json::BalanceAndPosition> const &) override;
  void operator()(Trace<json::Positions> const &) override;
  void operator()(Trace<json::Orders> const &) override;
  void operator()(Trace<json::OrderAck> const &) override;
  void operator()(Trace<json::AmendOrderAck> const &) override;
  void operator()(Trace<json::CancelOrderAck> const &) override;

  void check_subscribe_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  size_t const index_;
  // web socket
  std::unique_ptr<web::socket::Client> const connection_;
  // buffers
  core::Buffer decode_buffer_;
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
  // account
  Account &account_;
  // cache
  Shared &shared_;
  absl::flat_hash_set<Symbol> all_symbols_;  // only master (index 0)
  // state
  ConnectionStatus status_ = {};
  core::Download<MarketDataState> download_;
  // queue
  core::TimerQueue<std::string> subscribe_queue_;
};

}  // namespace okx
}  // namespace roq
