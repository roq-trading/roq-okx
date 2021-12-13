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
    : key_(key), hmac_(create_hmac(secret)), passphrase_(passphrase), secret_(secret) {
}

std::string Hasher::create_sign(const std::string_view &timestamp) {
  hmac_.clear();
  hmac_.update(timestamp);
  hmac_.update("GET/users/self/verify"sv);
  hmac_.update(secret_);
  std::array<char, 32> buffer;
  auto length = hmac_.digest(buffer);
  assert(length == std::size(buffer));
  auto result = core::binascii::Base64::encode(buffer, false);
  return result;
}

}  // namespace tools
}  // namespace okex
}  // namespace roq
