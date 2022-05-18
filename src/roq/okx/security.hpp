/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/http/method.hpp"

#include "roq/okx/config.hpp"

#include "roq/okx/tools/hasher.hpp"

namespace roq {
namespace okx {

class Security final {
 public:
  Security() {}
  Security(Config const &, std::string_view const &account);

  Security(Security &&) = delete;
  Security(Security const &) = delete;

  bool empty() const { return std::empty(account_); }

  std::string_view get_account() const { return account_; }

  std::string_view get_key() const { return hasher_.get_key(); }
  std::string_view get_passphrase() const { return hasher_.get_passphrase(); }

  std::string create_sign(std::string_view const &timestamp) { return hasher_.create_sign(timestamp); }

  std::string create_headers(core::http::Method, std::string_view const &path, std::string_view const &body);

 private:
  const std::string account_;
  tools::Hasher hasher_;
};

}  // namespace okx
}  // namespace roq
