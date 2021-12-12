/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"
#include "roq/core/json/parser.h"

#include "roq/server.h"

#include "roq/okex/json/error.h"
#include "roq/okex/json/subscribe.h"
#include "roq/okex/json/unsubscribe.h"

#include "roq/okex/json/spot_depth_l2_tbt.h"
#include "roq/okex/json/spot_ticker.h"
#include "roq/okex/json/spot_trade.h"

#include "roq/okex/json/action.h"
#include "roq/okex/json/table.h"

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
    virtual void operator()(server::Trace<json::SpotTicker> const &) = 0;
    virtual void operator()(server::Trace<json::SpotTrade> const &) = 0;
    virtual void operator()(server::Trace<json::SpotDepthL2Tbt> const &, Action) = 0;
  };

  static bool dispatch(
      Handler &handler,
      std::string_view const &message,
      core::json::Buffer &,
      server::TraceInfo const &);

 private:
  static bool dispatch_table(
      Handler &handler,
      std::string_view const &message,
      core::json::Buffer &,
      server::TraceInfo const &,
      Table,
      Action);
};

}  // namespace json
}  // namespace okex
}  // namespace roq
