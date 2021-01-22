/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <absl/flags/declare.h>

#include <cstdint>
#include <string>

ABSL_DECLARE_FLAG(std::string, config_file);

ABSL_DECLARE_FLAG(std::string, rest_uri);
ABSL_DECLARE_FLAG(std::string, ws_uri);
ABSL_DECLARE_FLAG(uint32_t, request_queue_depth);
ABSL_DECLARE_FLAG(uint32_t, request_timeout_secs);
ABSL_DECLARE_FLAG(uint32_t, rate_limit_interval_secs);
ABSL_DECLARE_FLAG(uint32_t, rate_limit_max_requests);
ABSL_DECLARE_FLAG(uint32_t, ping_freq_secs);
ABSL_DECLARE_FLAG(uint32_t, download_timeout_secs);
ABSL_DECLARE_FLAG(std::string, exchange);
ABSL_DECLARE_FLAG(bool, cancel_on_disconnect);
ABSL_DECLARE_FLAG(uint32_t, max_trades);
ABSL_DECLARE_FLAG(uint32_t, encode_buffer_size);
ABSL_DECLARE_FLAG(uint32_t, decode_buffer_size);
ABSL_DECLARE_FLAG(uint32_t, reconnect_secs);

ABSL_DECLARE_FLAG(uint32_t, max_batch_size);

ABSL_DECLARE_FLAG(bool, log_fix);

ABSL_DECLARE_FLAG(std::string, rest_ping_path);

// external

ABSL_DECLARE_FLAG(std::string, name);
ABSL_DECLARE_FLAG(uint32_t, cache_mbp_max_depth);
