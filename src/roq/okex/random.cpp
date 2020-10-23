/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/random.h"

#include <fmt/format.h>

#include <array>
#include <random>

#include "roq/core/binascii/hex.h"

#include "roq/core/crypto/sha.h"

namespace roq {
namespace okex {

static char CHARSET_DATA[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

constexpr auto CHARSET_LENGTH = sizeof(CHARSET_DATA) - 1;

static_assert(CHARSET_LENGTH == 62);

static std::default_random_engine GENERATOR;
static std::uniform_int_distribution<int> DISTRIBUTION(0, CHARSET_LENGTH);

constexpr size_t RANDOM_BYTES = 15;

Random::Random(const std::string_view &secret)
    : _hmac(secret.data(), secret.length()) {
}

std::string Random::create_nonce() {
  std::string result(RANDOM_BYTES, '-');
  std::generate(result.begin(), result.end(), []() {
    return CHARSET_DATA[DISTRIBUTION(GENERATOR)];
  });
  return result;
}

std::string Random::create_signature(const std::string_view &nonce) {
  _hmac.clear();
  _hmac.update(nonce);
  std::array<char, 32> buffer;
  auto length = _hmac.digest(buffer.data(), buffer.size());
  assert(length == buffer.size());
  return core::binascii::Hex::encode(buffer.data(), length);
}

}  // namespace okex
}  // namespace roq
