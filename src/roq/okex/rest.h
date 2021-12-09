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

#include "roq/okex/json/currencies.h"
#include "roq/okex/json/order_book.h"
#include "roq/okex/json/symbols.h"
#include "roq/okex/json/token.h"

namespace roq {
namespace okex {

class Rest final : public core::web::Client::Handler {
 public:
  struct PublicToken final {
    std::string_view uri;
    std::string_view query;
    std::chrono::nanoseconds ping_frequency = {};
  };

  struct SymbolsUpdate final {
    std::vector<std::string> &symbols;
  };

  struct Handler {
    virtual void operator()(server::Trace<StreamStatus> const &) = 0;
    virtual void operator()(server::Trace<ExternalLatency> const &) = 0;
    virtual void operator()(server::Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(server::Trace<MarketStatus> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(PublicToken const &) = 0;
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

  void get_public_token();
  void get_public_token_ack(const server::Trace<core::web::Response> &, uint32_t sequence);
  void operator()(const server::Trace<json::Token> &);

  void get_currencies();
  void get_currencies_ack(const server::Trace<core::web::Response> &, uint32_t sequence);
  void operator()(const server::Trace<json::Currencies> &);

  void get_symbols();
  void get_symbols_ack(const server::Trace<core::web::Response> &, uint32_t sequence);
  void operator()(const server::Trace<json::Symbols> &);

  void get_order_book(const std::string_view &symbol);
  void get_order_book_ack(
      const server::Trace<core::web::Response> &, const std::string_view &symbol);
  void operator()(server::Trace<json::OrderBook> const &, const std::string_view &symbol);

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
    core::metrics::Profile public_token, public_token_ack,  //
        currencies, currencies_ack,                         //
        symbols, symbols_ack,                               //
        order_book, order_book_ack;
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
  std::chrono::nanoseconds refresh_token_ = {};
};

}  // namespace okex
}  // namespace roq
