/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/tools/hasher.hpp"

#include <fmt/format.h>

#include <array>

#include "roq/logging.hpp"

#include "roq/utils/chrono.hpp"

#include "roq/core/iso_datetime.hpp"

#include "roq/core/binascii/base64.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace tools {

namespace {
auto create_hmac(const std::string_view &secret) {
  return core::crypto::HMAC_SHA256(secret);
}
}  // namespace

Hasher::Hasher(
    const std::string_view &key, const std::string_view &secret, const std::string_view &passphrase)
    : key_(key), hmac_(create_hmac(secret)), passphrase_(passphrase), secret_(secret) {
}

std::string Hasher::create_sign(const std::string_view &timestamp) {
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
    core::http::Method method,
    const std::string_view &path,
    const std::string_view &body,
    std::chrono::milliseconds timestamp) {
  assert(!std::empty(path));
  auto tmp = fmt::format("{}"sv, core::to_iso8601(timestamp));
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
