/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/utils/metrics/counter.hpp"
#include "roq/utils/metrics/latency.hpp"
#include "roq/utils/metrics/profile.hpp"

#include "roq/io/context.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/server.hpp"

#include "roq/server/stream.hpp"

#include "roq/okx/gateway/shared.hpp"

#include "roq/okx/protocol/json/candles_ack.hpp"
#include "roq/okx/protocol/json/instruments_ack.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct Rest final : public Base<Rest>, public server::Stream, public web::rest::Client::Handler {
  struct SymbolsUpdate final {
    std::span<Symbol const> symbols;
  };

  struct Handler {
    virtual void operator()(SymbolsUpdate &) = 0;
  };

  Rest(Handler &, io::Context &context, uint16_t stream_id, Shared &);

  // protected:
  friend base_type;

  // server::Stream

  uint16_t stream_id() const override { return stream_id_; }

  bool ready() const override { return connection_status_ == ConnectionStatus::READY; }

  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(metrics::Writer &) const override;

  void operator()(Trace<ConnectionStatus> const &, std::string_view const &reason = {}) override;

 protected:
  // web::rest::Client::Handler

  void operator()(Trace<web::rest::Connected> const &) override;
  void operator()(Trace<web::rest::Disconnected> const &) override;
  void operator()(Trace<web::rest::Latency> const &) override;

  // instruments

  void get_instruments(std::string_view const &type);
  void get_instruments_ack(Trace<web::rest::Response> const &, std::string_view const &type);
  void operator()(Trace<protocol::json::InstrumentsAck> const &);

  // candles

  void get_candles(std::string_view const &symbol);
  void get_candles_ack(Trace<web::rest::Response> const &, std::string_view const &symbol);
  void operator()(Trace<protocol::json::CandlesAck> const &, std::string_view const &symbol);

  // helpers

  void check_request_queue(std::chrono::nanoseconds now);
  bool downloading() const { return download_instruments_.spot || download_instruments_.swap || download_instruments_.futures; }

  void process_response(Trace<web::rest::Response> const &, auto error_handler, auto success_handler);

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

}  // namespace gateway
}  // namespace okx
}  // namespace roq
