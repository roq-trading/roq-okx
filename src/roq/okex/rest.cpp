/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/rest.h"

#include <absl/flags/flag.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "roq/core/json/parser.h"

#include "roq/okex/gateway.h"
#include "roq/okex/options.h"

#include "roq/okex/json/utils.h"

namespace roq {
namespace okex {

namespace {
constexpr std::string_view CONNECTION = "rest";

static auto create_counter(const std::string_view &function) {
  return core::metrics::Counter(
      absl::GetFlag(FLAGS_name), CONNECTION, function);
}

static auto create_profile(const std::string_view &function) {
  return core::metrics::Profile(
      absl::GetFlag(FLAGS_name), CONNECTION, function);
}

static auto create_latency(const std::string_view &function) {
  return core::metrics::Latency(
      absl::GetFlag(FLAGS_name), CONNECTION, function);
}
}  // namespace

Rest::Rest(
    Gateway &gateway,
    [[maybe_unused]] const Config &config,
    Random &random,
    core::event::Base &base,
    core::event::DNSBase &dns_base,
    core::ssl::Context &ssl_context)
    : _gateway(gateway), _random(random),
      _connection(
          *this,
          base,
          dns_base,
          ssl_context,
          core::URI(absl::GetFlag(FLAGS_rest_uri)),
          ROQ_PACKAGE_NAME,
          true,  // keep alive
          absl::GetFlag(FLAGS_request_queue_depth),
          std::chrono::seconds{absl::GetFlag(FLAGS_request_timeout_secs)},
          std::chrono::seconds{absl::GetFlag(FLAGS_rate_limit_interval_secs)},
          absl::GetFlag(FLAGS_rate_limit_max_requests),
          std::chrono::seconds{absl::GetFlag(FLAGS_ping_freq_secs)},
          absl::GetFlag(FLAGS_decode_buffer_size),
          absl::GetFlag(FLAGS_encode_buffer_size),
          absl::GetFlag(FLAGS_rest_ping_path)),
      _decode_buffer(absl::GetFlag(FLAGS_decode_buffer_size)),
      _counter{
          .disconnect = create_counter("disconnect"),
      },
      _profile{}, _latency{
                      .ping = create_latency("ping"),
                  } {
}

bool Rest::ready() const {
  return _connection.ready();
}

void Rest::close() {
  _connection.close();
}

void Rest::operator()(const Event<Start> &) {
  _connection.start();
}

void Rest::operator()(const Event<Stop> &) {
  _connection.stop();
}

void Rest::operator()(const Event<Timer> &event) {
  _connection.refresh(event.value.now);
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(_counter.disconnect, metrics::COUNTER)
      // profile
      // latency
      .write(_latency.ping, metrics::LATENCY);
}

void Rest::operator()(const core::web::Client::Connected &) {
  _gateway(*this);
}

void Rest::operator()(const core::web::Client::Disconnected &) {
  ++_counter.disconnect;
  _gateway(*this);
}

void Rest::operator()(const core::web::Client::Latency &latency) {
  _latency.ping.update(
      std::chrono::duration_cast<std::chrono::nanoseconds>(latency.sample)
          .count());
}

}  // namespace okex
}  // namespace roq
