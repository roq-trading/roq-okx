/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/security.h"

#include "roq/utils/safe_cast.h"

#include "roq/core/clock.h"

namespace roq {
namespace okex {

Security::Security(const Config &config, const std::string_view &account)
    : account_(account), hasher_(
                             config.get_api_key(account_),
                             config.get_secret(account_),
                             config.get_passphrase(account_)) {
}

}  // namespace okex
}  // namespace roq
