/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/okx/tools/crypto.hpp"

#include <fmt/format.h>

#include <array>

#include "roq/logging.hpp"

#include "roq/utils/chrono.hpp"
#include "roq/utils/datetime.hpp"

#include "roq/utils/codec/base64.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace tools {

// === IMPLEMENTATION ===

Crypto::Crypto() : mac_{""sv} {
}

Crypto::Crypto(std::string_view const &key, std::string_view const &secret, std::string_view const &passphrase)
    : key_(key), mac_(secret), passphrase_(passphrase), secret_(secret) {
}

std::string Crypto::create_sign(std::string_view const &timestamp) {
  log::debug(R"(timestamp="{}", secret="{}")"sv, timestamp, secret_);
  mac_.clear();
  mac_.update(timestamp);
  mac_.update("GET/users/self/verify"sv);
  auto digest = mac_.final(digest_);
  std::string result;
  utils::codec::Base64::encode(result, digest, false, false);
  return result;
}

std::string Crypto::create_headers(
    web::http::Method method,
    std::string_view const &path,
    std::string_view const &body,
    std::chrono::milliseconds timestamp) {
  assert(!std::empty(path));
  auto tmp = fmt::format("{}"sv, utils::DateTime_iso8601{timestamp});
  auto tmp_2 = fmt::format("{}{}"sv, tmp, method);
  mac_.clear();
  mac_.update(tmp_2);
  mac_.update(path);
  mac_.update(body);
  auto digest = mac_.final(digest_);
  std::string signature;
  utils::codec::Base64::encode(signature, digest, false, false);
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
