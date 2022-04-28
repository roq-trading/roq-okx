/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/json/parser.hpp"

#include <utility>

#include "roq/logging.hpp"

#include "roq/okx/json/action.hpp"
#include "roq/okx/json/arg.hpp"
#include "roq/okx/json/event_type.hpp"

#include "roq/okx/json/frame.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

bool Parser::dispatch(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    TraceInfo const &trace_info) {
  auto frame = core::json::Parser::create<Frame>(message, buffer);
  switch (frame.op) {
    using enum Operation::type_t;
    case UNDEFINED:
      switch (frame.event) {
        using enum EventType::type_t;
        case UNDEFINED:
          switch (frame.arg.channel) {
            using enum Channel::type_t;
            case UNDEFINED:
              break;
            case UNKNOWN:
              assert(false);
              break;
            case STATUS:
              dispatch_event<Status>(handler, message, buffer, trace_info);
              return true;
            case INSTRUMENTS:
              dispatch_event_array<Instruments>(handler, message, buffer, trace_info);
              return true;
            case ESTIMATED_PRICE:
              dispatch_event<EstimatedPrice>(handler, message, buffer, trace_info);
              return true;
            case PRICE_LIMIT:
              dispatch_event<PriceLimit>(handler, message, buffer, trace_info);
              return true;
            case MARK_PRICE:
              dispatch_event<MarkPrice>(handler, message, buffer, trace_info);
              return true;
            case TICKERS:
              dispatch_event_array<Tickers>(handler, message, buffer, trace_info);
              return true;
            case TRADES:
              dispatch_event_array<Trades>(handler, message, buffer, trace_info);
              return true;
            case BOOKS_L2_TBT:
              dispatch_event<BooksL2Tbt>(
                  handler, message, buffer, trace_info, frame.arg.inst_id, frame.action);
              return true;
            case INDEX_TICKERS:
              dispatch_event_array<IndexTickers>(handler, message, buffer, trace_info);
              return true;
            case FUNDING_RATE:
              dispatch_event_array<FundingRate>(handler, message, buffer, trace_info);
              return true;
            case ACCOUNT:
              dispatch_event<Account>(handler, message, buffer, trace_info);
              return true;
            case BALANCE_AND_POSITION:
              dispatch_event<BalanceAndPosition>(handler, message, buffer, trace_info);
              return true;
            case POSITIONS:
              dispatch_event_array<Positions>(handler, message, buffer, trace_info);
              return true;
            case ORDERS:
              dispatch_event_frame<Orders>(handler, message, buffer, trace_info);
              return true;
          }
          break;
        case UNKNOWN:
          assert(false);
          break;
        case ERROR: {
          Error error;
          error.code = frame.code;
          error.msg = frame.msg;
          create_trace_and_dispatch(handler, trace_info, std::as_const(error));
          return true;
        }
        case LOGIN: {
          Login login;
          login.code = frame.code;
          login.msg = frame.msg;
          create_trace_and_dispatch(handler, trace_info, std::as_const(login));
          return true;
        }
        case SUBSCRIBE: {
          Subscribe subscribe;
          subscribe.channel = frame.arg.channel;
          subscribe.inst_id = frame.arg.inst_id;
          create_trace_and_dispatch(handler, trace_info, std::as_const(subscribe));
          return true;
        }
        case UNSUBSCRIBE: {
          Unsubscribe unsubscribe;
          unsubscribe.channel = frame.arg.channel;
          unsubscribe.inst_id = frame.arg.inst_id;
          create_trace_and_dispatch(handler, trace_info, std::as_const(unsubscribe));
          return true;
        }
      }
      break;
    case UNKNOWN:
      assert(false);
      break;
    case ORDER:
    case BATCH_ORDERS:
      dispatch_event_frame<OrderAck>(handler, message, buffer, trace_info);
      return true;
    case AMEND_ORDER:
    case BATCH_AMEND_ORDERS:
      dispatch_event_frame<AmendOrderAck>(handler, message, buffer, trace_info);
      return true;
    case CANCEL_ORDER:
    case BATCH_CANCEL_ORDERS:
      dispatch_event_frame<CancelOrderAck>(handler, message, buffer, trace_info);
      return true;
  }
  return false;
}

// note! each item of data dispatched independently
template <typename T, typename... Args>
void Parser::dispatch_event(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    TraceInfo const &trace_info,
    Args &&...args) {
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("data"sv) != 0)
      continue;
    for (auto item : std::get<core::json::Array>(value)) {
      const T obj{item, buffer};
      create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
    }
    break;
  }
}

// note! data as an array -- can *not* be nested
template <typename T, typename... Args>
void Parser::dispatch_event_array(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    TraceInfo const &trace_info,
    Args &&...args) {
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key.compare("data"sv) != 0)
      continue;
    const T obj{value, buffer};
    create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
    break;
  }
}

// note! the entire message
template <typename T, typename... Args>
void Parser::dispatch_event_frame(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    TraceInfo const &trace_info,
    Args &&...args) {
  const auto obj = core::json::Parser::create<T>(message, buffer);
  create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

}  // namespace json
}  // namespace okx
}  // namespace roq
