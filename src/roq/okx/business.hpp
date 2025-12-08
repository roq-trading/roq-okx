/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/download.hpp"
#include "roq/core/timer_queue.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/okx/shared.hpp"

#include "roq/okx/json/parser.hpp"

namespace roq {
namespace okx {

struct Business final : public web::socket::Client::Handler, public json::Parser::Handler {
  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<TimeSeriesUpdate> const &, bool is_last) = 0;
  };

  Business(Handler &, io::Context &, uint16_t stream_id, Shared &);

  Business(Business const &) = delete;

  uint16_t stream_id() const { return stream_id_; }

  bool ready() const { return status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

  void subscribe(size_t start_from = 0);

 protected:
  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

 private:
  void operator()(ConnectionStatus);

  void subscribe_static();

  void subscribe(std::span<Symbol const> const &symbols);

  void subscribe(std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values);

  void parse(std::string_view const &message);

  // json::Parser::Handler

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
  void operator()(Trace<json::BboTbt> const &) override;
  void operator()(Trace<json::BooksL2Tbt> const &) override;
  void operator()(Trace<json::IndexTickers> const &) override;
  void operator()(Trace<json::FundingRate> const &) override;

  void operator()(Trace<json::ChannelConnCount> const &) override;
  void operator()(Trace<json::Login> const &) override;
  void operator()(Trace<json::Account> const &) override;
  void operator()(Trace<json::BalanceAndPosition> const &) override;
  void operator()(Trace<json::Positions> const &) override;
  void operator()(Trace<json::Orders> const &) override;
  void operator()(Trace<json::Order> const &) override;
  void operator()(Trace<json::AmendOrder> const &) override;
  void operator()(Trace<json::CancelOrder> const &) override;

  void operator()(Trace<json::Candle> const &) override;

  // helpers

  void check_subscribe_queue(std::chrono::nanoseconds now);

 private:
  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // web socket
  std::unique_ptr<web::socket::Client> const connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile parse, error, subscribe, unsubscribe, candles;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // cache
  Shared &shared_;
  // state
  ConnectionStatus status_ = {};
  // queue
  core::TimerQueue<std::string> subscribe_queue_;
  // sequencing
  utils::unordered_map<std::string, int64_t> sequence_;
};

}  // namespace okx
}  // namespace roq
