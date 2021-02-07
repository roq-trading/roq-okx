/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/okex/flags.h"

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <string>

#include "roq/core/flags/non_empty.h"
#include "roq/core/flags/non_zero.h"
#include "roq/core/flags/path.h"
#include "roq/core/flags/uri.h"

ABSL_FLAG(  //
    roq::core::flags::Path<std::string>,
    config_file,
    {},
    "config file (path)");

ABSL_FLAG(  //
    roq::core::flags::URI<std::string>,
    rest_uri,
    {"https://api.okex.com/api/2"},
    "REST end-point (URI)");

ABSL_FLAG(  //
    roq::core::flags::URI<std::string>,
    ws_uri,
    {"wss://api.okex.com/api/2/ws"},
    "WebSocket end-point (URI)");

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    request_queue_depth,
    uint32_t{5},
    "request: max queue depth");

ABSL_FLAG(  //
    uint32_t,
    request_timeout_secs,
    uint32_t{30},
    "request: timeout (seconds)");

ABSL_FLAG(  //
    uint32_t,
    rate_limit_interval_secs,
    uint32_t{1},
    "rate limit: monitor interval (seconds)");

ABSL_FLAG(  //
    uint32_t,
    rate_limit_max_requests,
    uint32_t{300},
    "rate limit: max requests (per interval)");

ABSL_FLAG(  //
    uint32_t,
    ping_freq_secs,
    uint32_t{5},
    "ping frequency (seconds)");

ABSL_FLAG(  //
    uint32_t,
    download_timeout_secs,
    uint32_t{15},
    "download time-out (seconds)");

ABSL_FLAG(  //
    roq::core::flags::NonEmpty<std::string>,
    exchange,
    {"okex"},
    "exchange identifier (string)");

ABSL_FLAG(  //
    bool,
    cancel_on_disconnect,
    true,
    "cancel orders on disconnect? (bool)");

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    max_trades,
    uint32_t{16384},
    "maximum trades for trade summary");

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    encode_buffer_size,
    uint32_t{1048576},
    "encode buffer size");

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    decode_buffer_size,
    uint32_t{10485760},
    "decode buffer size");

ABSL_FLAG(  //
    uint32_t,
    reconnect_secs,
    uint32_t{10},
    "time before reconnect (seconds)");

ABSL_FLAG(  //
    roq::core::flags::NonZero<uint32_t>,
    max_batch_size,
    uint32_t{56},
    "max batch size (it appears there is a limit)");

ABSL_FLAG(  //
    bool,
    log_fix,
    false,
    "log fix messages?");

ABSL_FLAG(  //
    roq::core::flags::Path<std::string>,
    rest_ping_path,
    {"/public/ticker/BTCUSD"},
    "URI path used for REST connection keep-alive messages");

// external

ABSL_DECLARE_FLAG(roq::core::flags::NonEmpty<std::string>, name);
ABSL_DECLARE_FLAG(roq::core::flags::NonZero<uint32_t>, cache_mbp_max_depth);

namespace roq {
namespace okex {

std::string_view Flags::config_file() {
  static const std::string result = absl::GetFlag(FLAGS_config_file);
  return result;
}

std::string_view Flags::rest_uri() {
  static const std::string result = absl::GetFlag(FLAGS_rest_uri);
  return result;
}

std::string_view Flags::ws_uri() {
  static const std::string result = absl::GetFlag(FLAGS_ws_uri);
  return result;
}

uint32_t Flags::request_queue_depth() {
  static const uint32_t result = absl::GetFlag(FLAGS_request_queue_depth);
  return result;
}

uint32_t Flags::request_timeout_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_request_timeout_secs);
  return result;
}

uint32_t Flags::rate_limit_interval_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_rate_limit_interval_secs);
  return result;
}

uint32_t Flags::rate_limit_max_requests() {
  static const uint32_t result = absl::GetFlag(FLAGS_rate_limit_max_requests);
  return result;
}

uint32_t Flags::ping_freq_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_ping_freq_secs);
  return result;
}

uint32_t Flags::download_timeout_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_download_timeout_secs);
  return result;
}

std::string_view Flags::exchange() {
  static const std::string result = absl::GetFlag(FLAGS_exchange);
  return result;
}

bool Flags::cancel_on_disconnect() {
  static const bool result = absl::GetFlag(FLAGS_cancel_on_disconnect);
  return result;
}

uint32_t Flags::max_trades() {
  static const uint32_t result = absl::GetFlag(FLAGS_max_trades);
  return result;
}

uint32_t Flags::encode_buffer_size() {
  static const uint32_t result = absl::GetFlag(FLAGS_encode_buffer_size);
  return result;
}

uint32_t Flags::decode_buffer_size() {
  static const uint32_t result = absl::GetFlag(FLAGS_decode_buffer_size);
  return result;
}

uint32_t Flags::reconnect_secs() {
  static const uint32_t result = absl::GetFlag(FLAGS_reconnect_secs);
  return result;
}

uint32_t Flags::max_batch_size() {
  static const uint32_t result = absl::GetFlag(FLAGS_max_batch_size);
  return result;
}

bool Flags::log_fix() {
  static const bool result = absl::GetFlag(FLAGS_log_fix);
  return result;
}

std::string_view Flags::rest_ping_path() {
  static const std::string result = absl::GetFlag(FLAGS_rest_ping_path);
  return result;
}

std::string_view Flags::name() {
  static const std::string result = absl::GetFlag(FLAGS_name);
  return result;
}

uint32_t Flags::cache_mbp_max_depth() {
  static const uint32_t result = absl::GetFlag(FLAGS_cache_mbp_max_depth);
  return result;
}

}  // namespace okex
}  // namespace roq
