/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/okx/account.hpp"

#include "roq/logging.hpp"

#include "roq/utils/safe_cast.hpp"

#include "roq/clock.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === HELPERS ===

namespace {
auto create_crypto(auto &config, auto &name) -> tools::Crypto {
  auto ready = true;
  auto key = config.get_api_key(name);
  if (std::empty(key)) {
    ready = false;
    log::warn(R"(Unexpected: missing key for name="{}")"sv, name);
  }
  auto secret = config.get_secret(name);
  if (std::empty(secret)) {
    ready = false;
    log::warn(R"(Unexpected: missing secret for name="{}")"sv, name);
  }
  auto passphrase = config.get_passphrase(name);
  if (std::empty(passphrase)) {
    ready = false;
    log::warn(R"(Unexpected: missing passphrase for name="{}")"sv, name);
  }
  if (!ready)
    log::fatal("Invalid config"sv);
  return {key, secret, passphrase};
}
}  // namespace

// === IMPLEMENTATION ===

Account::Account(Config const &config, std::string_view const &name)
    : name_{name}, crypto_{create_crypto(config, name_)} {
}

std::string Account::create_headers(
    web::http::Method method, std::string_view const &path, std::string_view const &body) {
  auto timestamp = clock::get_realtime<std::chrono::milliseconds>();
  return crypto_.create_headers(method, path, body, timestamp);
}

}  // namespace okx
}  // namespace roq
