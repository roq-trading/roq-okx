/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/account.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/clock.hpp"

namespace roq {
namespace okx {

// === IMPLEMENTATION ===

Account::Account(Config const &config, std::string_view const &name)
    : name_{name}, crypto_{config.get_api_key(name_), config.get_secret(name_), config.get_passphrase(name_)} {
}

std::string Account::create_headers(
    web::http::Method method, std::string_view const &path, std::string_view const &body) {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  return crypto_.create_headers(method, path, body, timestamp);
}

}  // namespace okx
}  // namespace roq
