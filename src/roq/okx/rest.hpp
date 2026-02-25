/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "roq/utils/container.hpp"

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/download.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/okx/shared.hpp"

#include "roq/okx/json/candles_ack.hpp"
#include "roq/okx/json/instruments_ack.hpp"

namespace roq {
namespace okx {

struct Rest final : public web::rest::Client::Handler {
  struct SymbolsUpdate final {
    std::span<Symbol const> symbols;
  };

  struct Handler {
    virtual void operator()(Trace<StreamStatus> const &) = 0;
    virtual void operator()(Trace<ExternalLatency> const &) = 0;
    virtual void operator()(Trace<ReferenceData> const &, bool is_last) = 0;
    virtual void operator()(Trace<MarketStatus> const &, bool is_last) = 0;
    virtual void operator()(Trace<TimeSeriesUpdate> const &, bool is_last) = 0;
    // cross-communication
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  Rest(Handler &, io::Context &context, uint16_t stream_id, Shared &);

  Rest(Rest const &) = delete;

  bool ready() const { return connection_status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void operator()(metrics::Writer &) const;

 protected:
  // web::rest::Client::Handler

  void operator()(Trace<web::rest::Client::Connected> const &) override;
  void operator()(Trace<web::rest::Client::Disconnected> const &) override;
  void operator()(Trace<web::rest::Client::Latency> const &) override;

  void operator()(ConnectionStatus, std::string_view const &reason = {});

  bool downloading() const { return download_instruments_.spot || download_instruments_.swap || download_instruments_.futures; }

  // instruments

  void get_instruments(std::string_view const &type);
  void get_instruments_ack(Trace<web::rest::Response> const &, std::string_view const &type);
  void operator()(Trace<json::InstrumentsAck> const &);

  // candles

  void get_candles(std::string_view const &symbol);
  void get_candles_ack(Trace<web::rest::Response> const &, std::string_view const &symbol);
  void operator()(Trace<json::CandlesAck> const &, std::string_view const &symbol);

  // helpers

  void check_request_queue(std::chrono::nanoseconds now);

  void process_response(web::rest::Response const &, auto error_handler, auto success_handler);

 private:
  Handler &handler_;
  // config
  uint16_t const stream_id_;
  std::string const name_;
  // connection
  std::unique_ptr<web::rest::Client> const connection_;
  // buffers
  core::json::BufferStack decode_buffer_;
  // metrics
  struct {
    utils::metrics::Counter disconnect;
  } counter_;
  struct {
    utils::metrics::Profile instruments, instruments_ack, candles, candles_ack;
  } profile_;
  struct {
    utils::metrics::Latency ping;
  } latency_;
  // shared
  Shared &shared_;
  // state
  ConnectionStatus connection_status_ = {};
  struct {
    bool spot = {};
    bool swap = {};
    bool futures = {};
    // bool option = {};
  } download_instruments_;
};

}  // namespace okx
}  // namespace roq
