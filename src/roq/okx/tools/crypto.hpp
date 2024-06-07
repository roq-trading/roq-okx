/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <array>
#include <chrono>
#include <string>
#include <string_view>

#include "roq/web/http/method.hpp"

#include "roq/utils/mac/hmac.hpp"

namespace roq {
namespace okx {
namespace tools {

struct Crypto final {
  Crypto();
  Crypto(std::string_view const &key, std::string_view const &secret, std::string_view const &passphrase);

  Crypto(Crypto &&) = delete;
  Crypto(Crypto const &) = delete;

  std::string_view get_key() const { return key_; }
  std::string_view get_passphrase() const { return passphrase_; }

  std::string create_sign(std::string_view const &timestamp);

  std::string create_headers(web::http::Method, std::string_view const &path, std::string_view const &body, std::chrono::milliseconds timestamp);

 private:
  using MAC = utils::mac::HMAC<utils::hash::SHA256>;
  using Digest = std::array<std::byte, MAC::DIGEST_LENGTH>;

  std::string const key_;
  MAC mac_;
  Digest digest_;
  std::string const passphrase_;
  std::string const secret_;
};

}  // namespace tools
}  // namespace okx
}  // namespace roq
