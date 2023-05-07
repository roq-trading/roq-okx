/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/shared.hpp"

#include "roq/okx/flags.hpp"

namespace roq {
namespace okx {

// === IMPLEMENTATION ===

Shared::Shared(server::Dispatcher &dispatcher, Settings const &settings)
    : dispatcher_{dispatcher}, settings{settings},
      rate_limiter{Flags::request_limit(), Flags::request_limit_interval()},
      symbols{Flags::ws_max_subscriptions_per_stream()} {
}

}  // namespace okx
}  // namespace roq
