/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/protocol/json/parser.hpp"

#include "roq/logging.hpp"

#include "roq/utils/hash/fnv.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace protocol {
namespace json {

// === CONSTANTS ===

namespace {
constexpr auto const KEY_OP = "op"sv;
constexpr auto const KEY_EVENT = "event"sv;
constexpr auto const KEY_ARG = "arg"sv;
constexpr auto const KEY_CHANNEL = "channel"sv;
}  // namespace

// === HELPERS ===

namespace {
template <typename T>
auto dispatch_helper(auto &handler, auto &message, auto &buffer_stack, auto &trace_info) {
  T obj{message, buffer_stack};
  create_trace_and_dispatch(handler, trace_info, obj);
  return true;
}
}  // namespace

// === IMPLEMENTATION ===

bool Parser::dispatch(
    Handler &handler, std::string_view const &message, core::json::BufferStack &buffer_stack, TraceInfo const &trace_info, bool allow_unknown_event_types) {
  auto result = false;
  auto helper = [&](auto &key, auto &value) {
    auto key_2 = utils::hash::FNV::compute(key);
    switch (key_2) {
      case utils::hash::FNV::compute(KEY_OP): {
        Operation op{value};
        switch (op) {
          using enum Operation::type_t;
          case UNDEFINED_INTERNAL:
            log::fatal("Unexpected"sv);
          case UNKNOWN_INTERNAL:
            return true;
          case ORDER:
          case BATCH_ORDERS:
            result = dispatch_helper<Order>(handler, message, buffer_stack, trace_info);
            return true;
          case AMEND_ORDER:
          case BATCH_AMEND_ORDERS:
            result = dispatch_helper<AmendOrder>(handler, message, buffer_stack, trace_info);
            return true;
          case CANCEL_ORDER:
          case BATCH_CANCEL_ORDERS:
            result = dispatch_helper<CancelOrder>(handler, message, buffer_stack, trace_info);
            return true;
        }
        break;
      }
      case utils::hash::FNV::compute(KEY_EVENT): {
        EventType event{value};
        switch (event) {
          using enum EventType::type_t;
          case UNDEFINED_INTERNAL:
            log::fatal("Unexpected"sv);
          case UNKNOWN_INTERNAL:
            return true;
          case ERROR:
            result = dispatch_helper<Error>(handler, message, buffer_stack, trace_info);
            return true;
          case LOGIN:
            result = dispatch_helper<Login>(handler, message, buffer_stack, trace_info);
            return true;
          case SUBSCRIBE:
            result = dispatch_helper<Subscribe>(handler, message, buffer_stack, trace_info);
            return true;
          case UNSUBSCRIBE:
            result = dispatch_helper<Unsubscribe>(handler, message, buffer_stack, trace_info);
            return true;
          case CHANNEL_CONN_COUNT:
            result = dispatch_helper<ChannelConnCount>(handler, message, buffer_stack, trace_info);
            return true;
          case NOTICE:
            result = dispatch_helper<Notice>(handler, message, buffer_stack, trace_info);
            return true;
        }
        break;
      }
      case utils::hash::FNV::compute(KEY_ARG):
        for (auto [k, v] : std::get<core::json::Object>(value)) {
          auto k_2 = utils::hash::FNV::compute(k);
          switch (k_2) {
            case utils::hash::FNV::compute(KEY_CHANNEL): {
              Channel channel{v};
              switch (channel) {
                using enum Channel::type_t;
                case UNDEFINED_INTERNAL:
                  log::fatal("Unexpected"sv);
                case UNKNOWN_INTERNAL:
                  return true;
                case STATUS:
                  result = dispatch_helper<Status>(handler, message, buffer_stack, trace_info);
                  return true;
                case INSTRUMENTS:
                  result = dispatch_helper<Instruments>(handler, message, buffer_stack, trace_info);
                  return true;
                case ESTIMATED_PRICE:
                  result = dispatch_helper<EstimatedPrice>(handler, message, buffer_stack, trace_info);
                  return true;
                case PRICE_LIMIT:
                  result = dispatch_helper<PriceLimit>(handler, message, buffer_stack, trace_info);
                  return true;
                case MARK_PRICE:
                  result = dispatch_helper<MarkPrice>(handler, message, buffer_stack, trace_info);
                  return true;
                case TICKERS:
                  result = dispatch_helper<Tickers>(handler, message, buffer_stack, trace_info);
                  return true;
                case TRADES:
                  result = dispatch_helper<Trades>(handler, message, buffer_stack, trace_info);
                  return true;
                case BOOKS5:
                  result = dispatch_helper<BooksL2Tbt>(handler, message, buffer_stack, trace_info);
                  return true;
                case BBO_TBT:
                  result = dispatch_helper<BboTbt>(handler, message, buffer_stack, trace_info);
                  return true;
                case BOOKS:
                case BOOKS_L2_TBT:
                case BOOKS50_L2_TBT:
                  result = dispatch_helper<BooksL2Tbt>(handler, message, buffer_stack, trace_info);
                  return true;
                case INDEX_TICKERS:
                  result = dispatch_helper<IndexTickers>(handler, message, buffer_stack, trace_info);
                  return true;
                case FUNDING_RATE:
                  result = dispatch_helper<FundingRate>(handler, message, buffer_stack, trace_info);
                  return true;
                case ACCOUNT:
                  result = dispatch_helper<Account>(handler, message, buffer_stack, trace_info);
                  return true;
                case BALANCE_AND_POSITION:
                  result = dispatch_helper<BalanceAndPosition>(handler, message, buffer_stack, trace_info);
                  return true;
                case POSITIONS:
                  result = dispatch_helper<Positions>(handler, message, buffer_stack, trace_info);
                  return true;
                case ORDERS:
                  result = dispatch_helper<Orders>(handler, message, buffer_stack, trace_info);
                  return true;
                case CANDLE1M: {
                  result = dispatch_helper<Candle>(handler, message, buffer_stack, trace_info);
                  return true;
                }
              }
              break;
            }
          }
        }
        break;
    }
    return result;
  };
  core::json::Parser::dispatch<core::json::Object>(helper, message);
  if (result || allow_unknown_event_types) {
    return result;
  }
  log::fatal(R"(Unexpected: message="{}")"sv, message);
}

}  // namespace json
}  // namespace protocol
}  // namespace okx
}  // namespace roq
