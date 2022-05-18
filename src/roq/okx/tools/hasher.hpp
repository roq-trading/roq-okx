/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "roq/core/http/method.hpp"

#include "roq/core/crypto/hmac_sha256.hpp"

namespace roq {
namespace okx {
namespace tools {

class Hasher final {
 public:
  Hasher();
  Hasher(std::string_view const &key, std::string_view const &secret, std::string_view const &passphrase);

  Hasher(Hasher &&) = delete;
  Hasher(Hasher const &) = delete;

  std::string_view get_key() const { return key_; }
  std::string_view get_passphrase() const { return passphrase_; }

  std::string create_sign(std::string_view const &timestamp);

  std::string create_headers(
      core::http::Method,
      std::string_view const &path,
      std::string_view const &body,
      std::chrono::milliseconds timestamp);

 private:
  const std::string key_;
  core::crypto::HMAC_SHA256 hmac_;
  const std::string passphrase_;
  const std::string secret_;
};

}  // namespace tools
}  // namespace okx
}  // namespace roq
