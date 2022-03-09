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
    server::TraceInfo const &trace_info) {
  auto frame = core::json::Parser::create<Frame>(message, buffer);
  switch (frame.op) {
    case Operation::UNDEFINED:
      switch (frame.event) {
        case EventType::UNDEFINED:
          switch (frame.arg.channel) {
            case Channel::UNDEFINED:
              break;
            case Channel::UNKNOWN:
              assert(false);
              break;
            case Channel::STATUS:
              dispatch_event<Status>(handler, message, buffer, trace_info);
              return true;
            case Channel::INSTRUMENTS:
              dispatch_event_array<Instruments>(handler, message, buffer, trace_info);
              return true;
            case Channel::ESTIMATED_PRICE:
              dispatch_event<EstimatedPrice>(handler, message, buffer, trace_info);
              return true;
            case Channel::PRICE_LIMIT:
              dispatch_event<PriceLimit>(handler, message, buffer, trace_info);
              return true;
            case Channel::MARK_PRICE:
              dispatch_event<MarkPrice>(handler, message, buffer, trace_info);
              return true;
            case Channel::TICKERS:
              dispatch_event_array<Tickers>(handler, message, buffer, trace_info);
              return true;
            case Channel::TRADES:
              dispatch_event_array<Trades>(handler, message, buffer, trace_info);
              return true;
            case Channel::BOOKS_L2_TBT:
              dispatch_event<BooksL2Tbt>(
                  handler, message, buffer, trace_info, frame.arg.inst_id, frame.action);
              return true;
            case Channel::INDEX_TICKERS:
              dispatch_event_array<IndexTickers>(handler, message, buffer, trace_info);
              return true;
            case Channel::FUNDING_RATE:
              dispatch_event_array<FundingRate>(handler, message, buffer, trace_info);
              return true;
            case Channel::ACCOUNT:
              dispatch_event<Account>(handler, message, buffer, trace_info);
              return true;
            case Channel::BALANCE_AND_POSITION:
              dispatch_event<BalanceAndPosition>(handler, message, buffer, trace_info);
              return true;
            case Channel::POSITIONS:
              dispatch_event_array<Positions>(handler, message, buffer, trace_info);
              return true;
            case Channel::ORDERS:
              dispatch_event_frame<Orders>(handler, message, buffer, trace_info);
              return true;
          }
          break;
        case EventType::UNKNOWN:
          assert(false);
          break;
        case EventType::ERROR: {
          Error error;
          error.code = frame.code;
          error.msg = frame.msg;
          server::create_trace_and_dispatch(handler, trace_info, error);
          return true;
        }
        case EventType::LOGIN: {
          Login login;
          login.code = frame.code;
          login.msg = frame.msg;
          server::create_trace_and_dispatch(handler, trace_info, login);
          return true;
        }
        case EventType::SUBSCRIBE: {
          Subscribe subscribe;
          subscribe.channel = frame.arg.channel;
          subscribe.inst_id = frame.arg.inst_id;
          server::create_trace_and_dispatch(handler, trace_info, subscribe);
          return true;
        }
        case EventType::UNSUBSCRIBE: {
          Unsubscribe unsubscribe;
          unsubscribe.channel = frame.arg.channel;
          unsubscribe.inst_id = frame.arg.inst_id;
          server::create_trace_and_dispatch(handler, trace_info, unsubscribe);
          return true;
        }
      }
      break;
    case Operation::UNKNOWN:
      assert(false);
      break;
    case Operation::ORDER:
    case Operation::BATCH_ORDERS:
      dispatch_event_frame<OrderAck>(handler, message, buffer, trace_info);
      return true;
    case Operation::AMEND_ORDER:
    case Operation::BATCH_AMEND_ORDERS:
      dispatch_event_frame<AmendOrderAck>(handler, message, buffer, trace_info);
      return true;
    case Operation::CANCEL_ORDER:
    case Operation::BATCH_CANCEL_ORDERS:
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
    server::TraceInfo const &trace_info,
    Args &&...args) {
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::object_t>(root)) {
    if (key.compare("data"sv) != 0)
      continue;
    for (auto item : std::get<core::json::array_t>(value)) {
      T obj{item, buffer};
      server::create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
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
    server::TraceInfo const &trace_info,
    Args &&...args) {
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::object_t>(root)) {
    if (key.compare("data"sv) != 0)
      continue;
    T obj{value, buffer};
    server::create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
    break;
  }
}

// note! the entire message
template <typename T, typename... Args>
void Parser::dispatch_event_frame(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    server::TraceInfo const &trace_info,
    Args &&...args) {
  auto obj = core::json::Parser::create<T>(message, buffer);
  server::create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

}  // namespace json
}  // namespace okx
}  // namespace roq
