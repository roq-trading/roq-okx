/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/json/parser.h"

#include "roq/logging.h"

#include "roq/okex/json/action.h"
#include "roq/okex/json/arg.h"
#include "roq/okex/json/event_type.h"

using namespace std::literals;

namespace roq {
namespace okex {
namespace json {

bool Parser::dispatch(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    server::TraceInfo const &trace_info) {
  // all
  Channel channel;
  std::string_view inst_id, inst_type;
  // event
  EventType event;
  int64_t code = {};
  std::string_view msg;
  // push
  Action action;  // *ONLY* books
  // possibly two-pass
  for (int i = 0; i < 2; ++i) {
    core::json::Parser parser(message);
    auto root = parser.root();
    for (auto [key, value] : std::get<core::json::object_t>(root)) {
      if (key.compare("event"sv) == 0) {
        event = EventType{value};
      } else if (key.compare("arg"sv) == 0) {
        Arg arg{value};
        channel = arg.channel;
        inst_id = arg.inst_id;
        inst_type = arg.inst_type;
      } else if (key.compare("action"sv) == 0) {
        action = Action{value};
      } else if (key.compare("data"sv) == 0) {
        if (ROQ_UNLIKELY(event != EventType{}))
          log::fatal("Unexpected"sv);
        switch (channel) {
          case Channel::UNDEFINED:
            break;
          case Channel::UNKNOWN:
            assert(false);
            break;
          case Channel::INSTRUMENTS: {
            Instruments instruments{value, buffer};
            server::create_trace_and_dispatch(handler, trace_info, instruments);
            return true;
          }
          case Channel::TICKERS: {
            Tickers tickers{value, buffer};
            server::create_trace_and_dispatch(handler, trace_info, tickers);
            return true;
          }
          case Channel::TRADES: {
            Trades trades{value, buffer};
            server::create_trace_and_dispatch(handler, trace_info, trades);
            return true;
          }
          case Channel::BOOKS_L2_TBT: {
            if (action != Action{}) {
              if (!std::empty(inst_id)) {
                auto count = 0;
                for (auto item : std::get<core::json::array_t>(value)) {
                  ++count;
                  BooksL2Tbt books_l2_tbt{item, buffer};
                  server::create_trace_and_dispatch(
                      handler, trace_info, books_l2_tbt, inst_id, action);
                }
                if (count > 1)
                  log::warn("*** PLEASE REPORT: MORE BOOKS-L2-TBT ***"sv);
                return true;
              } else {
                log::warn("*** NO INST_ID ??? ***"sv);
                return false;
              }
            }
            break;
          }
          case Channel::ACCOUNT: {
            auto count = 0;
            for (auto item : std::get<core::json::array_t>(value)) {
              ++count;
              Account account{item, buffer};
              server::create_trace_and_dispatch(handler, trace_info, account);
            }
            if (count > 1)
              log::warn("*** PLEASE REPORT: MORE ACCOUNT ***"sv);
            return true;
            break;
          }
          case Channel::BALANCE_AND_POSITION: {
            auto count = 0;
            for (auto item : std::get<core::json::array_t>(value)) {
              ++count;
              BalanceAndPosition balance_and_position{item, buffer};
              server::create_trace_and_dispatch(handler, trace_info, balance_and_position);
            }
            if (count > 1)
              log::warn("*** PLEASE REPORT: MORE BALANCE_AND_POSITION ***"sv);
            return true;
            break;
          }
          case Channel::POSITIONS: {
            Positions positions{value, buffer};
            server::create_trace_and_dispatch(handler, trace_info, positions);
            return true;
          }
          case Channel::ORDERS: {
            Orders orders{value, buffer};
            server::create_trace_and_dispatch(handler, trace_info, orders);
            return true;
          }
        }
      } else if (key.compare("code"sv) == 0) {
        // msg = std::get<int32_t>(value); // XXX HANS missing
      } else if (key.compare("msg"sv) == 0) {
        msg = std::get<std::string_view>(value);
      } else {
        log::fatal(R"(Unexpected: key="{}")"sv, key);
      }
    }
    switch (event) {
      case EventType::UNDEFINED:
        // need one more pass:
        //   data must have arrived before arg (maybe action)
        break;
      case EventType::UNKNOWN:
        assert(false);
        break;
      case EventType::ERROR: {
        Error error;
        error.code = code;
        error.msg = msg;
        server::create_trace_and_dispatch(handler, trace_info, error);
        return true;
      }
      case EventType::SUBSCRIBE: {
        Subscribe subscribe;
        subscribe.channel = channel;
        subscribe.inst_id = inst_id;
        server::create_trace_and_dispatch(handler, trace_info, subscribe);
        return true;
      }
      case EventType::UNSUBSCRIBE: {
        Unsubscribe unsubscribe;
        unsubscribe.channel = channel;
        unsubscribe.inst_id = inst_id;
        server::create_trace_and_dispatch(handler, trace_info, unsubscribe);
        return true;
      }
      case EventType::LOGIN: {
        Login login;
        login.code = code;  // note! could be lost -- do we care?
        login.msg = msg;    // note! could be lost -- do we care?
        server::create_trace_and_dispatch(handler, trace_info, login);
        return true;
      }
    }
  }
  return false;
}

}  // namespace json
}  // namespace okex
}  // namespace roq
