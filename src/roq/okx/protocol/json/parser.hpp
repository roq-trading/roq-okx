/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/trace_info.hpp"

#include "roq/core/json/buffer_stack.hpp"

#include "roq/okx/protocol/json/error.hpp"
#include "roq/okx/protocol/json/subscribe.hpp"
#include "roq/okx/protocol/json/unsubscribe.hpp"

#include "roq/okx/protocol/json/bbo_tbt.hpp"
#include "roq/okx/protocol/json/books_l2_tbt.hpp"
#include "roq/okx/protocol/json/estimated_price.hpp"
#include "roq/okx/protocol/json/funding_rate.hpp"
#include "roq/okx/protocol/json/index_tickers.hpp"
#include "roq/okx/protocol/json/instruments.hpp"
#include "roq/okx/protocol/json/mark_price.hpp"
#include "roq/okx/protocol/json/price_limit.hpp"
#include "roq/okx/protocol/json/status.hpp"
#include "roq/okx/protocol/json/tickers.hpp"
#include "roq/okx/protocol/json/trades.hpp"

#include "roq/okx/protocol/json/candle.hpp"

#include "roq/okx/protocol/json/account.hpp"
#include "roq/okx/protocol/json/balance_and_position.hpp"
#include "roq/okx/protocol/json/channel_conn_count.hpp"
#include "roq/okx/protocol/json/login.hpp"
#include "roq/okx/protocol/json/orders.hpp"
#include "roq/okx/protocol/json/positions.hpp"

#include "roq/okx/protocol/json/amend_order.hpp"
#include "roq/okx/protocol/json/cancel_order.hpp"
#include "roq/okx/protocol/json/order.hpp"

#include "roq/okx/protocol/json/action.hpp"

namespace roq {
namespace okx {
namespace protocol {
namespace json {

struct Parser final {
  struct Handler {
    // events
    virtual void operator()(Trace<protocol::json::Error> const &) = 0;
    virtual void operator()(Trace<protocol::json::Subscribe> const &) = 0;
    virtual void operator()(Trace<protocol::json::Unsubscribe> const &) = 0;
    // push
    // - public
    virtual void operator()(Trace<protocol::json::Status> const &) = 0;
    virtual void operator()(Trace<protocol::json::Instruments> const &) = 0;
    virtual void operator()(Trace<protocol::json::EstimatedPrice> const &) = 0;
    virtual void operator()(Trace<protocol::json::PriceLimit> const &) = 0;
    virtual void operator()(Trace<protocol::json::MarkPrice> const &) = 0;
    virtual void operator()(Trace<protocol::json::Tickers> const &) = 0;
    virtual void operator()(Trace<protocol::json::Trades> const &) = 0;
    virtual void operator()(Trace<protocol::json::BboTbt> const &) = 0;
    virtual void operator()(Trace<protocol::json::BooksL2Tbt> const &) = 0;
    virtual void operator()(Trace<protocol::json::IndexTickers> const &) = 0;
    virtual void operator()(Trace<protocol::json::FundingRate> const &) = 0;
    // - private
    // -- admin
    virtual void operator()(Trace<protocol::json::ChannelConnCount> const &) = 0;
    // -- event
    virtual void operator()(Trace<protocol::json::Login> const &) = 0;
    virtual void operator()(Trace<protocol::json::Account> const &) = 0;
    virtual void operator()(Trace<protocol::json::BalanceAndPosition> const &) = 0;
    virtual void operator()(Trace<protocol::json::Positions> const &) = 0;
    virtual void operator()(Trace<protocol::json::Orders> const &) = 0;
    // -- ack
    virtual void operator()(Trace<protocol::json::Order> const &) = 0;
    virtual void operator()(Trace<protocol::json::AmendOrder> const &) = 0;
    virtual void operator()(Trace<protocol::json::CancelOrder> const &) = 0;
    // - business
    virtual void operator()(Trace<protocol::json::Candle> const &) = 0;
  };

  static bool dispatch(Handler &, std::string_view const &message, core::json::BufferStack &, TraceInfo const &, bool allow_unknown_event_types);
};

}  // namespace json
}  // namespace protocol
}  // namespace okx
}  // namespace roq
