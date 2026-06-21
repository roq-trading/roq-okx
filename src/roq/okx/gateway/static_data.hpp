/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>
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

#include "roq/okx/gateway/account.hpp"
#include "roq/okx/gateway/shared.hpp"

#include "roq/okx/protocol/json/parser.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct StaticData final : public web::socket::Client::Handler, public protocol::json::Parser::Handler {
  struct SymbolsUpdate final {
    std::vector<Symbol> &symbols;
  };

  struct Handler {
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  StaticData(Handler &, io::Context &, uint16_t stream_id, Account &, Shared &);

  StaticData(StaticData const &) = delete;

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

 protected:
  // web::socket::Client::Handler

  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

  // helpers

  uint16_t stream_id() const { return stream_id_; }

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  enum class State {
    UNDEFINED = 0,
    LOGIN,
    DONE,
  };

  uint32_t download(State);

  void login();

  void subscribe_static();

  void subscribe(std::string_view const &channel);
  void subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value);

  void parse(std::string_view const &message);

  // protocol::json::Parser::Handler

  void operator()(Trace<protocol::json::Error> const &) override;
  void operator()(Trace<protocol::json::Subscribe> const &) override;
  void operator()(Trace<protocol::json::Unsubscribe> const &) override;

  void operator()(Trace<protocol::json::Status> const &) override;
  void operator()(Trace<protocol::json::Instruments> const &) override;
  void operator()(Trace<protocol::json::EstimatedPrice> const &) override;
  void operator()(Trace<protocol::json::PriceLimit> const &) override;
  void operator()(Trace<protocol::json::MarkPrice> const &) override;
  void operator()(Trace<protocol::json::Tickers> const &) override;
  void operator()(Trace<protocol::json::Trades> const &) override;
  void operator()(Trace<protocol::json::BboTbt> const &) override;
  void operator()(Trace<protocol::json::BooksL2Tbt> const &) override;
  void operator()(Trace<protocol::json::IndexTickers> const &) override;
  void operator()(Trace<protocol::json::FundingRate> const &) override;

  void operator()(Trace<protocol::json::ChannelConnCount> const &) override;
  void operator()(Trace<protocol::json::Login> const &) override;
  void operator()(Trace<protocol::json::Account> const &) override;
  void operator()(Trace<protocol::json::BalanceAndPosition> const &) override;
  void operator()(Trace<protocol::json::Positions> const &) override;
  void operator()(Trace<protocol::json::Orders> const &) override;
  void operator()(Trace<protocol::json::Order> const &) override;
  void operator()(Trace<protocol::json::AmendOrder> const &) override;
  void operator()(Trace<protocol::json::CancelOrder> const &) override;

  void operator()(Trace<protocol::json::Candle> const &) override;

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
    utils::metrics::Profile parse, error, subscribe, unsubscribe, login, status, instruments;
  } profile_;
  struct {
    utils::metrics::Latency ping, heartbeat;
  } latency_;
  // account
  Account &account_;
  // cache
  Shared &shared_;
  // state
  ConnectionStatus connection_status_ = {};
  core::Download<State> download_;
  // queue
  core::TimerQueue<std::string> subscribe_queue_;
  // sequencing
  utils::unordered_map<std::string, int64_t> sequence_;
};

}  // namespace gateway
}  // namespace okx
}  // namespace roq
