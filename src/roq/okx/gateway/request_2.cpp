/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/gateway/request_2.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace gateway {

// === IMPLEMENTATION ===

Request2::Request2(Shared &shared) : instruments{*this}, shared_{shared} {
}

bool Request2::ready() const {
  return instruments.ready();
}

bool Request2::can_request(TraceInfo const &trace_info) {
  return shared_.rate_limiter.can_request(trace_info);
}

}  // namespace gateway
}  // namespace okx
}  // namespace roq
