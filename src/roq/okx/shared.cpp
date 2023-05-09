/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/shared.hpp"

namespace roq {
namespace okx {

// === IMPLEMENTATION ===

Shared::Shared(server::Dispatcher &dispatcher, Settings const &settings)
    : dispatcher_{dispatcher}, settings{settings},
      rate_limiter{settings.common.request_limit, settings.common.request_limit_interval},
      symbols{settings.ws.max_subscriptions_per_stream} {
}

}  // namespace okx
}  // namespace roq
