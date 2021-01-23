/* Copyright (c) 2017-2021, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>

#include "roq/core/crypto/hmac.h"

namespace roq {
namespace okex {

class Random final {
 public:
  explicit Random(const std::string_view &secret);

  Random(Random &&) = delete;
  Random(const Random &) = delete;

  std::string create_nonce();
  std::string create_signature(const std::string_view &nonce);

 private:
  core::crypto::HMAC_SHA256 _hmac;
};

}  // namespace okex
}  // namespace roq
