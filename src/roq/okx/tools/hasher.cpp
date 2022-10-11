/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/tools/hasher.hpp"

#include <fmt/format.h>

#include <array>

#include "roq/logging.hpp"

#include "roq/utils/chrono.hpp"
#include "roq/utils/datetime.hpp"

#include "roq/core/binascii/base64.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace tools {

// === HELPERS ===

namespace {
auto create_hmac(auto const &secret) {
  return core::crypto::HMAC_SHA256{secret};
}
}  // namespace

// === IMPLEMENTATION ===

Hasher::Hasher() : hmac_(create_hmac(""sv)) {
}

Hasher::Hasher(std::string_view const &key, std::string_view const &secret, std::string_view const &passphrase)
    : key_(key), hmac_(create_hmac(secret)), passphrase_(passphrase), secret_(secret) {
}

std::string Hasher::create_sign(std::string_view const &timestamp) {
  log::debug(R"(timestamp="{}", secret="{}")"sv, timestamp, secret_);
  hmac_.clear();
  hmac_.update(timestamp);
  hmac_.update("GET/users/self/verify"sv);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == std::size(buffer));
  auto result = core::binascii::Base64::encode(buffer, false);
  return result;
}

std::string Hasher::create_headers(
    web::http::Method method,
    std::string_view const &path,
    std::string_view const &body,
    std::chrono::milliseconds timestamp) {
  assert(!std::empty(path));
  auto tmp = fmt::format("{}"sv, utils::DateTime_iso8601{timestamp});
  auto tmp_2 = fmt::format("{}{}"sv, tmp, method);
  hmac_.clear();
  hmac_.update(tmp_2);
  hmac_.update(path);
  hmac_.update(body);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == std::size(buffer));
  auto signature = core::binascii::Base64::encode(buffer, false);
  auto result = fmt::format(
      "OK-ACCESS-KEY: {}\r\n"
      "OK-ACCESS-SIGN: {}\r\n"
      "OK-ACCESS-TIMESTAMP: {}\r\n"
      "OK-ACCESS-PASSPHRASE: {}\r\n"sv,
      key_,
      signature,
      tmp,
      passphrase_);
  return result;
}

}  // namespace tools
}  // namespace okx
}  // namespace roq
