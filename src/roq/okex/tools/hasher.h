/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "roq/core/http/method.h"

#include "roq/core/crypto/hmac.h"

namespace roq {
namespace okex {
namespace tools {

class Hasher final {
 public:
  Hasher(
      const std::string_view &key,
      const std::string_view &secret,
      const std::string_view &passphrase);

  Hasher(Hasher &&) = delete;
  Hasher(const Hasher &) = delete;

  std::string create_headers(
      core::http::Method,
      const std::string_view &path,
      const std::string_view &body,
      std::chrono::milliseconds now);

 private:
  const std::string key_;
  core::crypto::HMAC_SHA256 hmac_;
  const std::string passphrase_;
};

}  // namespace tools
}  // namespace okex
}  // namespace roq
