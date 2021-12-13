/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.h"
#include "roq/core/json/parser.h"

#include "roq/server.h"

#include "roq/okex/json/error.h"
#include "roq/okex/json/subscribe.h"
#include "roq/okex/json/unsubscribe.h"

#include "roq/okex/json/books_l2_tbt.h"
#include "roq/okex/json/instruments.h"
#include "roq/okex/json/tickers.h"
#include "roq/okex/json/trades.h"

#include "roq/okex/json/action.h"

#include "roq/okex/json/login.h"

namespace roq {
namespace okex {
namespace json {

struct Parser final {
  struct Handler {
    // events
    virtual void operator()(server::Trace<json::Error> const &) = 0;
    virtual void operator()(server::Trace<json::Subscribe> const &) = 0;
    virtual void operator()(server::Trace<json::Unsubscribe> const &) = 0;
    // push
    // - public
    virtual void operator()(server::Trace<json::Instruments> const &) = 0;
    virtual void operator()(server::Trace<json::Tickers> const &) = 0;
    virtual void operator()(server::Trace<json::Trades> const &) = 0;
    virtual void operator()(
        server::Trace<json::BooksL2Tbt> const &, const std::string_view &inst_id, Action) = 0;
    // - private
    virtual void operator()(server::Trace<json::Login> const &) = 0;
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
