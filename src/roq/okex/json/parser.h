/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/parser.h"

#include "roq/server.h"

#include "roq/okex/json/ack.h"
#include "roq/okex/json/error.h"
#include "roq/okex/json/pong.h"
#include "roq/okex/json/welcome.h"

#include "roq/okex/json/level2.h"
#include "roq/okex/json/snapshot.h"
#include "roq/okex/json/ticker.h"

#include "roq/okex/json/account_balance.h"
#include "roq/okex/json/order_change.h"

namespace roq {
namespace okex {
namespace json {

struct Parser final {
  struct Handler {
    virtual void operator()(server::Trace<json::Welcome> const &) = 0;
    virtual void operator()(server::Trace<json::Error> const &) = 0;
    virtual void operator()(server::Trace<json::Pong> const &) = 0;
    virtual void operator()(server::Trace<json::Ack> const &) = 0;

    virtual void operator()(server::Trace<json::Snapshot> const &) = 0;
    virtual void operator()(server::Trace<json::Ticker> const &) = 0;
    virtual void operator()(server::Trace<json::Level2> const &) = 0;

    virtual void operator()(server::Trace<json::AccountBalance> const &) = 0;
    virtual void operator()(server::Trace<json::OrderChange> const &) = 0;
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
