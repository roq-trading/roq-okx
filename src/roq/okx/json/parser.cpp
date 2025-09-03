/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/okx/json/parser.hpp"

#include <utility>

#include "roq/logging.hpp"

#include "roq/core/json/array_parser.hpp"

#include "roq/okx/json/action.hpp"
#include "roq/okx/json/arg.hpp"
#include "roq/okx/json/event_type.hpp"

#include "roq/okx/json/frame.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

// === HELPERS ===

namespace {
auto create_candle(auto &message, auto &buffer_stack) {
  Candle result;
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key == "arg"sv) {
      result.arg = value;
    } else if (key == "data"sv) {
      if (!core::json::is_null(value)) {
        result.data = core::json::ArrayParser<decltype(result.data), core::json::Array>::parse(buffer_stack, std::get<core::json::Array>(value));
      }
    }
  }
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

bool Parser::dispatch(Handler &handler, std::string_view const &message, core::json::BufferStack &buffer_stack, TraceInfo const &trace_info) {
  Frame frame{message, buffer_stack};
  switch (frame.op) {
    using enum Operation::type_t;
    case UNDEFINED_INTERNAL:
      switch (frame.event) {
        using enum EventType::type_t;
        case UNDEFINED_INTERNAL:
          switch (frame.arg.channel) {
            using enum Channel::type_t;
            case UNDEFINED_INTERNAL:
              break;
            case UNKNOWN_INTERNAL:
              assert(false);
              break;
            case STATUS:
              dispatch_event<Status>(handler, message, buffer_stack, trace_info);
              return true;
            case INSTRUMENTS:
              dispatch_event_array<Instruments>(handler, message, buffer_stack, trace_info);
              return true;
            case ESTIMATED_PRICE:
              dispatch_event<EstimatedPrice>(handler, message, buffer_stack, trace_info);
              return true;
            case PRICE_LIMIT:
              dispatch_event<PriceLimit>(handler, message, buffer_stack, trace_info);
              return true;
            case MARK_PRICE:
              dispatch_event<MarkPrice>(handler, message, buffer_stack, trace_info);
              return true;
            case TICKERS:
              dispatch_event_array<Tickers>(handler, message, buffer_stack, trace_info);
              return true;
            case TRADES:
              dispatch_event_array<Trades>(handler, message, buffer_stack, trace_info);
              return true;
            case BOOKS5:
              dispatch_event<BooksL2Tbt>(handler, message, buffer_stack, trace_info, frame.arg.inst_id, json::Action::SNAPSHOT);
              return true;
            case BBO_TBT:
              // note! these updates appear to always be snapshot
              dispatch_event<BboTbt>(handler, message, buffer_stack, trace_info, frame.arg.inst_id);
              return true;
            case BOOKS:
            case BOOKS_L2_TBT:
            case BOOKS50_L2_TBT:
              dispatch_event<BooksL2Tbt>(handler, message, buffer_stack, trace_info, frame.arg.inst_id, frame.action);
              return true;
            case INDEX_TICKERS:
              dispatch_event_array<IndexTickers>(handler, message, buffer_stack, trace_info);
              return true;
            case FUNDING_RATE:
              dispatch_event_array<FundingRate>(handler, message, buffer_stack, trace_info);
              return true;
            case ACCOUNT:
              dispatch_event<Account>(handler, message, buffer_stack, trace_info);
              return true;
            case BALANCE_AND_POSITION:
              dispatch_event<BalanceAndPosition>(handler, message, buffer_stack, trace_info);
              return true;
            case POSITIONS:
              dispatch_event_array<Positions>(handler, message, buffer_stack, trace_info);
              return true;
            case ORDERS:
              dispatch_event_frame<Orders>(handler, message, buffer_stack, trace_info);
              return true;
            case CANDLE1M: {
              auto candle = create_candle(message, buffer_stack);
              create_trace_and_dispatch(handler, trace_info, candle);
              return true;
            }
          }
          break;
        case UNKNOWN_INTERNAL:
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
          subscribe.inst_type = frame.arg.inst_type;
          subscribe.inst_id = frame.arg.inst_id;
          create_trace_and_dispatch(handler, trace_info, std::as_const(subscribe));
          return true;
        }
        case UNSUBSCRIBE: {
          Unsubscribe unsubscribe;
          unsubscribe.channel = frame.arg.channel;
          unsubscribe.inst_type = frame.arg.inst_type;
          unsubscribe.inst_id = frame.arg.inst_id;
          create_trace_and_dispatch(handler, trace_info, std::as_const(unsubscribe));
          return true;
        }
        case CHANNEL_CONN_COUNT: {
          ChannelConnCount channel_conn_count;
          channel_conn_count.channel = frame.channel;
          channel_conn_count.conn_count = frame.conn_count;
          channel_conn_count.conn_id = frame.conn_id;
          create_trace_and_dispatch(handler, trace_info, std::as_const(channel_conn_count));
          return true;
        }
      }
      break;
    case UNKNOWN_INTERNAL:
      assert(false);
      break;
    case ORDER:
    case BATCH_ORDERS:
      dispatch_event_frame<OrderAck>(handler, message, buffer_stack, trace_info);
      return true;
    case AMEND_ORDER:
    case BATCH_AMEND_ORDERS:
      dispatch_event_frame<AmendOrderAck>(handler, message, buffer_stack, trace_info);
      return true;
    case CANCEL_ORDER:
    case BATCH_CANCEL_ORDERS:
      dispatch_event_frame<CancelOrderAck>(handler, message, buffer_stack, trace_info);
      return true;
  }
  return false;
}

// note! each item of data dispatched independently
template <typename T, typename... Args>
void Parser::dispatch_event(auto &handler, auto &message, auto &buffer_stack, auto &trace_info, Args &&...args) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != "data"sv) {
      continue;
    }
    for (auto item : std::get<core::json::Array>(value)) {
      T obj{item, buffer_stack};
      create_trace_and_dispatch(handler, trace_info, obj, args...);  // XXX FIXME TODO std::forward ???
    }
    break;
  }
}

// note! data as an array -- can *not* be nested
template <typename T, typename... Args>
void Parser::dispatch_event_array(auto &handler, auto &message, auto &buffer_stack, auto &trace_info, Args &&...args) {
  core::json::Parser parser{message};
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::Object>(root)) {
    if (key != "data"sv) {
      continue;
    }
    T obj{value, buffer_stack};
    create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
    break;
  }
}

// note! the entire message
template <typename T, typename... Args>
void Parser::dispatch_event_frame(auto &handler, auto &message, auto &buffer_stack, auto &trace_info, Args &&...args) {
  T obj{message, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj, std::forward<Args>(args)...);
}

}  // namespace json
}  // namespace okx
}  // namespace roq
