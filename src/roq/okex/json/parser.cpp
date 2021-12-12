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
    default:
      return dispatch_table(
          handler, message, buffer, trace_info, event_or_table.table, event_or_table.action);
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

// note! data and spot/depth_l2_tbt creates nested arrays
bool Parser::dispatch_table(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    server::TraceInfo const &trace_info,
    Table table,
    Action action) {
  core::json::Parser parser(message);
  auto root = parser.root();
  for (auto [key, value] : std::get<core::json::object_t>(root)) {
    if (key.compare("data"sv) == 0) {
      switch (table) {
        case Table::UNDEFINED:
        case Table::UNKNOWN:
          log::fatal("Unexpected"sv);
          std::abort();
        case Table::SPOT_TICKER: {
          for (auto item : std::get<core::json::array_t>(value)) {
            json::SpotTicker spot_ticker{item, buffer};
            server::create_trace_and_dispatch(handler, trace_info, spot_ticker);
          }
          return true;
        }
        case Table::SPOT_TRADE: {
          for (auto item : std::get<core::json::array_t>(value)) {
            json::SpotTrade spot_trade{item, buffer};
            server::create_trace_and_dispatch(handler, trace_info, spot_trade);
          }
          return true;
        }
        case Table::SPOT_DEPTH_L2_TBT: {
          for (auto item : std::get<core::json::array_t>(value)) {
            json::SpotDepthL2Tbt spot_depth_l2_tbt{item, buffer};
            server::create_trace_and_dispatch(handler, trace_info, spot_depth_l2_tbt, action);
          }
          return true;
        }
      }
    }
  }
  log::fatal("Unexpected"sv);
  std::abort();
  return false;
}

}  // namespace json
}  // namespace okex
}  // namespace roq
