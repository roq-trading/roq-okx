/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/web/http/method.hpp"

#include "roq/okx/config.hpp"

#include "roq/okx/tools/crypto.hpp"

namespace roq {
namespace okx {

struct Account final {
  Account() = default;
  Account(Config const &, std::string_view const &name);

  Account(Account const &) = delete;

  bool empty() const { return std::empty(name); }

  std::string_view get_key() const { return crypto_.get_key(); }
  std::string_view get_passphrase() const { return crypto_.get_passphrase(); }

  std::string create_sign(std::string_view const &timestamp) { return crypto_.create_sign(timestamp); }

  std::string create_headers(web::http::Method, std::string_view const &path, std::string_view const &body);

  std::string const name;

 private:
  tools::Crypto crypto_;
};

}  // namespace okx
}  // namespace roq
