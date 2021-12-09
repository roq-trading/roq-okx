/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/json/parser.h"

#include "roq/logging.h"

#include "roq/okex/json/message.h"

using namespace std::literals;

namespace roq {
namespace okex {
namespace json {

bool Parser::dispatch(
    Handler &handler,
    std::string_view const &message,
    core::json::Buffer &buffer,
    server::TraceInfo const &trace_info) {
  core::json::Parser parser(message);
  auto root = parser.root();
  json::Message message_(root, buffer);
  switch (message_.type) {
    case json::Type::UNDEFINED:
    case json::Type::UNKNOWN:
      log::fatal("Unexpected"sv);
      break;
    case json::Type::WELCOME: {
      core::json::Parser parser(message);
      auto root = parser.root();
      json::Welcome welcome(root, buffer);
      server::create_trace_and_dispatch(handler, trace_info, welcome);
      break;
    }
    case json::Type::ERROR: {
      core::json::Parser parser(message);
      auto root = parser.root();
      json::Error error(root, buffer);
      server::create_trace_and_dispatch(handler, trace_info, error);
      break;
    }
    case json::Type::PONG: {
      core::json::Parser parser(message);
      auto root = parser.root();
      json::Pong pong(root, buffer);
      server::create_trace_and_dispatch(handler, trace_info, pong);
      break;
    }
    case json::Type::ACK: {
      core::json::Parser parser(message);
      auto root = parser.root();
      json::Ack ack(root, buffer);
      server::create_trace_and_dispatch(handler, trace_info, ack);
      break;
    }
    case json::Type::MESSAGE:
      switch (message_.subject) {
        case json::Subject::UNDEFINED:
        case json::Subject::UNKNOWN:
          log::fatal("Unexpected"sv);
          break;
        case json::Subject::TRADE_SNAPSHOT: {
          core::json::Parser parser(message);
          auto root = parser.root();
          json::Snapshot snapshot(root, buffer);
          server::create_trace_and_dispatch(handler, trace_info, snapshot);
          break;
        }
        case json::Subject::TRADE_TICKER: {
          core::json::Parser parser(message);
          auto root = parser.root();
          json::Ticker ticker(root, buffer);
          server::create_trace_and_dispatch(handler, trace_info, ticker);
          break;
        }
        case json::Subject::TRADE_L2UPDATE: {
          core::json::Parser parser(message);
          auto root = parser.root();
          json::Level2 level2(root, buffer);
          server::create_trace_and_dispatch(handler, trace_info, level2);
          break;
        }
        case json::Subject::ACCOUNT_BALANCE: {
          core::json::Parser parser(message);
          auto root = parser.root();
          json::AccountBalance account_balance(root, buffer);
          server::create_trace_and_dispatch(handler, trace_info, account_balance);
          break;
        }
        case json::Subject::ORDER_CHANGE: {
          core::json::Parser parser(message);
          auto root = parser.root();
          json::OrderChange order_change(root, buffer);
          server::create_trace_and_dispatch(handler, trace_info, order_change);
          break;
        }
      }
      break;
  }
  return true;
}

}  // namespace json
}  // namespace okex
}  // namespace roq
