/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/shared.h"

#include "roq/okex/flags.h"

namespace roq {
namespace okex {

Shared::Shared(server::Dispatcher &dispatcher)
    : bids(server::Flags::cache_mbp_max_depth()), asks(server::Flags::cache_mbp_max_depth()),
      final_bids(server::Flags::cache_mbp_max_depth()),
      final_asks(server::Flags::cache_mbp_max_depth()),
      trades(server::Flags::cache_trades_max_depth()), dispatcher_(dispatcher),
      rate_limiter_(Flags::request_limit(), Flags::request_limit_interval()), generic_buffer(4096) {
}

}  // namespace okex
}  // namespace roq
