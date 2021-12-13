/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string>
#include <string_view>

#include "roq/core/http/method.h"

#include "roq/okex/config.h"

#include "roq/okex/tools/hasher.h"

namespace roq {
namespace okex {

class Security final {
 public:
  Security(const Config &, const std::string_view &account);

  Security(Security &&) = delete;
  Security(const Security &) = delete;

  std::string_view get_account() const { return account_; }

  std::string_view get_key() const { return hasher_.get_key(); }
  std::string_view get_passphrase() const { return hasher_.get_passphrase(); }

  std::string create_sign(const std::string_view &timestamp) {
    return hasher_.create_sign(timestamp);
  }

 private:
  const std::string account_;
  tools::Hasher hasher_;
};

}  // namespace okex
}  // namespace roq
