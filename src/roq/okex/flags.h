/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <cstdint>
#include <string_view>

namespace roq {
namespace okex {

struct Flags final {
  static std::string_view config_file();
  static std::string_view rest_uri();
  static std::string_view ws_uri();
  static uint32_t request_queue_depth();
  static uint32_t request_timeout_secs();
  static uint32_t rate_limit_interval_secs();
  static uint32_t rate_limit_max_requests();
  static uint32_t ping_freq_secs();
  static uint32_t download_timeout_secs();
  static std::string_view exchange();
  static bool cancel_on_disconnect();
  static uint32_t max_trades();
  static uint32_t encode_buffer_size();
  static uint32_t decode_buffer_size();
  static uint32_t reconnect_secs();
  static uint32_t max_batch_size();
  static bool log_fix();
  static std::string_view rest_ping_path();
  // external
  static std::string_view name();
  static uint32_t cache_mbp_max_depth();
};

}  // namespace okex
}  // namespace roq
