/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/gateway/request.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace gateway {

// === IMPLEMENTATION ===

Request::Request(Shared &shared) : account{*this}, balance{*this}, positions{*this}, orders{*this}, trades{*this}, shared_{shared} {
}

bool Request::ready() const {
  return account.ready() && balance.ready() && positions.ready() && orders.ready() && trades.ready();
}

bool Request::can_request(TraceInfo const &trace_info) {
  return shared_.rate_limiter.can_request(trace_info);
}

}  // namespace gateway
}  // namespace okx
}  // namespace roq
