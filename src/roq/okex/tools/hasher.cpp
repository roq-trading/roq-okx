/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/tools/hasher.h"

#include <fmt/format.h>

#include <array>

#include "roq/utils/chrono.h"

#include "roq/core/binascii/base64.h"

#include "roq/core/crypto/sha.h"

using namespace std::literals;

namespace roq {
namespace okex {
namespace tools {

namespace {
static auto create_hmac(const std::string_view &secret) {
  return core::crypto::HMAC_SHA256(secret);
}
}  // namespace

Hasher::Hasher(
    const std::string_view &key, const std::string_view &secret, const std::string_view &passphrase)
    : key_(key), hmac_(create_hmac(secret)), passphrase_(passphrase) {
}

std::string Hasher::create_headers(
    core::http::Method method,
    const std::string_view &path,
    const std::string_view &body,
    std::chrono::milliseconds timestamp) {
  assert(!std::empty(path));
  auto [date, time] = utils::split<decltype(timestamp)>(timestamp);
  auto timestamp_str = fmt::format(
      "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z"sv,
      static_cast<int>(date.year()),
      static_cast<unsigned>(date.month()),
      static_cast<unsigned>(date.day()),
      time.hours().count(),
      time.minutes().count(),
      time.seconds().count(),
      timestamp.count() % 1000);
  auto tmp = fmt::format("{}{}{}{}"sv, timestamp_str, method, path, body);
  hmac_.clear();
  hmac_.update(tmp);
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
      timestamp_str,
      passphrase_);
  return result;
}

}  // namespace tools
}  // namespace okex
}  // namespace roq
