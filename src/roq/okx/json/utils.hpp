/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/core/charconv/datetime.hpp"

#include "roq/okx/json/instrument_type.hpp"
#include "roq/okx/json/option_type.hpp"
#include "roq/okx/json/order_state.hpp"
#include "roq/okx/json/side.hpp"
#include "roq/okx/json/state.hpp"

namespace roq {
namespace okx {
namespace json {

template <typename T>
inline void update(T &result, const core::json::Value &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(std::chrono::milliseconds &result, const core::json::Value &value) {
  return std::visit(
      overloaded{
          [&](const core::json::Null &) { result = std::chrono::milliseconds{}; },
          [](bool) { throw std::bad_cast(); },
          [&](int64_t value) { result = std::chrono::milliseconds{value}; },
          [&](double value) { result = std::chrono::milliseconds{static_cast<int64_t>(value)}; },
          [&](const std::string_view &value) {
            result =
                core::charconv::datetime_from_string<std::remove_reference<decltype(result)>::type>(
                    value);
          },
          [](const core::json::Object &) { throw std::bad_cast(); },
          [](const core::json::Array &) { throw std::bad_cast(); },
      },
      value);
}

template <>
inline void update(std::chrono::nanoseconds &result, const core::json::Value &value) {
  return std::visit(
      overloaded{
          [&](const core::json::Null &) { result = std::chrono::nanoseconds{}; },
          [](bool) { throw std::bad_cast(); },
          [&](int64_t value) { result = std::chrono::nanoseconds{value}; },
          [&](double value) { result = std::chrono::nanoseconds{static_cast<int64_t>(value)}; },
          [&](const std::string_view &value) {
            result =
                core::charconv::datetime_from_string<std::remove_reference<decltype(result)>::type>(
                    value);
          },
          [](const core::json::Object &) { throw std::bad_cast(); },
          [](const core::json::Array &) { throw std::bad_cast(); },
      },
      value);
}

inline std::string_view strip_symbol_from_topic(const std::string_view &topic) {
  auto pos = topic.find_last_of(':');
  return pos == topic.npos ? topic : topic.substr(pos + 1);
}

// side

inline roq::Side map(json::Side value) {
  switch (value) {
    using enum json::Side::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  return {};
}

inline json::Side map(roq::Side value) {
  switch (value) {
    using enum roq::Side;
    case UNDEFINED:
      break;
    case BUY:
      return json::Side::BUY;
    case SELL:
      return json::Side::SELL;
  }
  return {};
}

// instrument type

inline roq::SecurityType map(json::InstrumentType value) {
  switch (value) {
    using enum json::InstrumentType::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case SPOT:
      return roq::SecurityType::SPOT;
    case MARGIN:
      return roq::SecurityType::SPOT;
    case SWAP:
      return roq::SecurityType::SWAP;
    case FUTURES:
      return roq::SecurityType::FUTURES;
    case OPTION:
      return roq::SecurityType::OPTION;
  }
  return {};
}

// option type

inline roq::OptionType map(json::OptionType value) {
  switch (value) {
    using enum json::OptionType::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case CALL:
      return roq::OptionType::CALL;
    case PUT:
      return roq::OptionType::PUT;
  }
  return {};
}

// state

inline roq::TradingStatus map(json::State value) {
  switch (value) {
    using enum json::State::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case LIVE:
      return roq::TradingStatus::OPEN;
    case SUSPENDED:
      return roq::TradingStatus::HALT;
    case PREOPEN:
      return roq::TradingStatus::PRE_OPEN;
    case SETTLEMENT:
      break;
  }
  return {};
}

// order state

inline roq::OrderStatus map(json::OrderState value) {
  switch (value) {
    using enum json::OrderState::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case LIVE:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
  }
  return {};
}

/*
// ord type

inline roq::OrderType map(json::OrdType value) {
  switch (value) {
    using enum json::OrdType::type_t;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case MARKET:
      return roq::OrderType::MARKET;
    case LIMIT:
      return roq::OrderType::LIMIT;
  }
  return {};
}
*/

// error

extern roq::Error guess_error(int32_t code);

}  // namespace json
}  // namespace okx
}  // namespace roq
