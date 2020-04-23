/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/options.h"

DEFINE_string(listen,
    "",
    "bind address (path)");
// DEFINE_validator(listen, ...);

DEFINE_string(config_file,
    "",
    "config file (path)");

DEFINE_string(rest_uri,
    "https://api.okex.com/api/2",
    "REST end-point (URI)");

DEFINE_string(ws_uri,
    "wss://api.okex.com/api/2/ws",
    "WebSocket end-point (URI)");

DEFINE_uint32(rate_limit_interval_secs,
    uint32_t{1},
    "rate limit: monitor interval (seconds)");

DEFINE_uint32(rate_limit_max_requests,
    uint32_t{300},
    "rate limit: max requests (per interval)");

DEFINE_uint32(ping_freq_secs,
    uint32_t{5},
    "ping frequency (seconds)");

DEFINE_uint32(download_timeout_secs,
    uint32_t{15},
    "download time-out (seconds)");

DEFINE_string(exchange,
    "okex",
    "exchange identifier (string)");

DEFINE_bool(cancel_on_disconnect,
    true,
    "cancel orders on disconnect? (bool)");

DEFINE_uint32(max_trades,
    uint32_t{16384},
    "maximum trades for trade summary");

DEFINE_uint32(encode_buffer_size,
    uint32_t{1048576},
    "encode buffer size");

DEFINE_uint32(decode_buffer_size,
    uint32_t{10485760},
    "decode buffer size");

DEFINE_uint32(reconnect_secs,
    {10},
    "time before reconnect (seconds)");

DEFINE_uint32(max_batch_size,
    56,
    "max batch size (it appears there is a limit)");

DEFINE_bool(log_fix,
    false,
    "log fix messages?");

DEFINE_string(rest_ping_path,
    "/public/ticker/BTCUSD",
    "URI path used for REST connection keep-alive messages");
