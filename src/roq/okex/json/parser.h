/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"
#include "roq/core/json/parser.h"

#include "roq/server.h"

#include "roq/okex/json/error.h"
#include "roq/okex/json/subscribe.h"
#include "roq/okex/json/unsubscribe.h"

namespace roq {
namespace okex {
namespace json {

struct Parser final {
  struct Handler {
    // events
    virtual void operator()(server::Trace<json::Error> const &) = 0;
    virtual void operator()(server::Trace<json::Subscribe> const &) = 0;
    virtual void operator()(server::Trace<json::Unsubscribe> const &) = 0;
    // tables
  };

  static bool dispatch(
      Handler &handler,
      std::string_view const &message,
      core::json::Buffer &,
      server::TraceInfo const &);
};

}  // namespace json
}  // namespace okex
}  // namespace roq
