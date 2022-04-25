/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/core/json/buffer.hpp"
#include "roq/core/json/parser.hpp"

#include "roq/server.hpp"

#include "roq/okx/json/arg.hpp"

#include "roq/okx/json/error.hpp"
#include "roq/okx/json/subscribe.hpp"
#include "roq/okx/json/unsubscribe.hpp"

#include "roq/okx/json/books_l2_tbt.hpp"
#include "roq/okx/json/estimated_price.hpp"
#include "roq/okx/json/funding_rate.hpp"
#include "roq/okx/json/index_tickers.hpp"
#include "roq/okx/json/instruments.hpp"
#include "roq/okx/json/mark_price.hpp"
#include "roq/okx/json/price_limit.hpp"
#include "roq/okx/json/status.hpp"
#include "roq/okx/json/tickers.hpp"
#include "roq/okx/json/trades.hpp"

#include "roq/okx/json/action.hpp"

#include "roq/okx/json/account.hpp"
#include "roq/okx/json/balance_and_position.hpp"
#include "roq/okx/json/login.hpp"
#include "roq/okx/json/orders.hpp"
#include "roq/okx/json/positions.hpp"

#include "roq/okx/json/amend_order_ack.hpp"
#include "roq/okx/json/cancel_order_ack.hpp"
#include "roq/okx/json/order_ack.hpp"

namespace roq {
namespace okx {
namespace json {

struct Parser final {
  struct Handler {
    // events
    virtual void operator()(Trace<json::Error const> const &) = 0;
    virtual void operator()(Trace<json::Subscribe const> const &) = 0;
    virtual void operator()(Trace<json::Unsubscribe const> const &) = 0;
    // push
    // - public
    virtual void operator()(Trace<json::Status const> const &) = 0;
    virtual void operator()(Trace<json::Instruments const> const &) = 0;
    virtual void operator()(Trace<json::EstimatedPrice const> const &) = 0;
    virtual void operator()(Trace<json::PriceLimit const> const &) = 0;
    virtual void operator()(Trace<json::MarkPrice const> const &) = 0;
    virtual void operator()(Trace<json::Tickers const> const &) = 0;
    virtual void operator()(Trace<json::Trades const> const &) = 0;
    virtual void operator()(
        Trace<json::BooksL2Tbt const> const &, const std::string_view &inst_id, Action) = 0;
    virtual void operator()(Trace<json::IndexTickers const> const &) = 0;
    virtual void operator()(Trace<json::FundingRate const> const &) = 0;
    // - private
    // -- event
    virtual void operator()(Trace<json::Login const> const &) = 0;
    virtual void operator()(Trace<json::Account const> const &) = 0;
    virtual void operator()(Trace<json::BalanceAndPosition const> const &) = 0;
    virtual void operator()(Trace<json::Positions const> const &) = 0;
    virtual void operator()(Trace<json::Orders const> const &) = 0;
    // -- ack
    virtual void operator()(Trace<json::OrderAck const> const &) = 0;
    virtual void operator()(Trace<json::AmendOrderAck const> const &) = 0;
    virtual void operator()(Trace<json::CancelOrderAck const> const &) = 0;
  };

  static bool dispatch(
      Handler &, std::string_view const &message, core::json::Buffer &, TraceInfo const &);

 private:
  template <typename T, typename... Args>
  static void dispatch_event(
      Handler &,
      std::string_view const &message,
      core::json::Buffer &,
      TraceInfo const &,
      Args &&...);

  template <typename T, typename... Args>
  static void dispatch_event_array(
      Handler &,
      std::string_view const &message,
      core::json::Buffer &,
      TraceInfo const &,
      Args &&...args);

  template <typename T, typename... Args>
  static void dispatch_event_frame(
      Handler &,
      std::string_view const &message,
      core::json::Buffer &,
      TraceInfo const &,
      Args &&...args);
};

}  // namespace json
}  // namespace okx
}  // namespace roq
