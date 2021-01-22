/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/options.h"

#include <absl/flags/flag.h>

ABSL_FLAG(std::string, config_file, "", "config file (path)");

ABSL_FLAG(
    std::string,
    rest_uri,
    "https://api.okex.com/api/2",
    "REST end-point (URI)");

ABSL_FLAG(
    std::string,
    ws_uri,
    "wss://api.okex.com/api/2/ws",
    "WebSocket end-point (URI)");

ABSL_FLAG(
    uint32_t, request_queue_depth, uint32_t{5}, "request: max queue depth");

ABSL_FLAG(
    uint32_t, request_timeout_secs, uint32_t{30}, "request: timeout (seconds)");

ABSL_FLAG(
    uint32_t,
    rate_limit_interval_secs,
    uint32_t{1},
    "rate limit: monitor interval (seconds)");

ABSL_FLAG(
    uint32_t,
    rate_limit_max_requests,
    uint32_t{300},
    "rate limit: max requests (per interval)");

ABSL_FLAG(uint32_t, ping_freq_secs, uint32_t{5}, "ping frequency (seconds)");

ABSL_FLAG(
    uint32_t,
    download_timeout_secs,
    uint32_t{15},
    "download time-out (seconds)");

ABSL_FLAG(std::string, exchange, "okex", "exchange identifier (string)");

ABSL_FLAG(
    bool, cancel_on_disconnect, true, "cancel orders on disconnect? (bool)");

ABSL_FLAG(
    uint32_t, max_trades, uint32_t{16384}, "maximum trades for trade summary");

ABSL_FLAG(
    uint32_t, encode_buffer_size, uint32_t{1048576}, "encode buffer size");

ABSL_FLAG(
    uint32_t, decode_buffer_size, uint32_t{10485760}, "decode buffer size");

ABSL_FLAG(uint32_t, reconnect_secs, {10}, "time before reconnect (seconds)");

ABSL_FLAG(
    uint32_t,
    max_batch_size,
    56,
    "max batch size (it appears there is a limit)");

ABSL_FLAG(bool, log_fix, false, "log fix messages?");

ABSL_FLAG(
    std::string,
    rest_ping_path,
    "/public/ticker/BTCUSD",
    "URI path used for REST connection keep-alive messages");
