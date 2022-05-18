/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/shared.hpp"

#include "roq/okx/flags.hpp"

namespace roq {
namespace okx {

Shared::Shared(server::Dispatcher &dispatcher)
    : bids(server::Flags::cache_mbp_max_depth()), asks(server::Flags::cache_mbp_max_depth()),
      final_bids(server::Flags::cache_mbp_max_depth()), final_asks(server::Flags::cache_mbp_max_depth()),
      trades(server::Flags::cache_trades_max_depth()), dispatcher_(dispatcher),
      rate_limiter(Flags::request_limit(), Flags::request_limit_interval()),
      symbols(Flags::ws_max_subscriptions_per_stream()) {
}

}  // namespace okx
}  // namespace roq
