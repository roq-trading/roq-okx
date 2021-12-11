/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/json/parser.h"

#include "roq/logging.h"

#include "roq/okex/json/event_or_table.h"

using namespace std::literals;

namespace roq {
namespace okex {
namespace json {

bool Parser::dispatch(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    server::TraceInfo const &trace_info) {
  auto event_or_table = core::json::Parser::create<json::EventOrTable>(message);
  // note! table is the more likely update
  switch (event_or_table.table) {
    case Table::UNDEFINED:
      break;  // then try table
    case Table::UNKNOWN:
      log::warn("Unknown table"sv);
      return false;
    case Table::SPOT_TICKER: {
      log::warn("MUST PARSE"sv);
      return true;
    }
  }
  switch (event_or_table.event) {
    case EventType::UNDEFINED:
      log::warn("Expected an event-type"sv);
      return false;
    case EventType::UNKNOWN:
      log::warn("Unknown event-type"sv);
      return false;
    case EventType::ERROR: {
      Error error;
      error.message = event_or_table.message;
      server::create_trace_and_dispatch(handler, trace_info, error);
      break;
    }
    case EventType::SUBSCRIBE: {
      Subscribe subscribe;
      subscribe.channel = event_or_table.channel;
      server::create_trace_and_dispatch(handler, trace_info, subscribe);
      break;
    }
    case EventType::UNSUBSCRIBE: {
      Unsubscribe unsubscribe;
      unsubscribe.channel = event_or_table.channel;
      server::create_trace_and_dispatch(handler, trace_info, unsubscribe);
      break;
    }
  }
  return true;
}

}  // namespace json
}  // namespace okex
}  // namespace roq
