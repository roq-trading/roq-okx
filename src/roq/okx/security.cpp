/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/security.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/core/clock.hpp"

namespace roq {
namespace okx {

Security::Security(Config const &config, std::string_view const &account)
    : account_(account),
      hasher_(config.get_api_key(account_), config.get_secret(account_), config.get_passphrase(account_)) {
}

std::string Security::create_headers(
    core::http::Method method, std::string_view const &path, std::string_view const &body) {
  auto timestamp = core::clock::GetRealTime<std::chrono::milliseconds>();
  return hasher_.create_headers(method, path, body, timestamp);
}

}  // namespace okx
}  // namespace roq
