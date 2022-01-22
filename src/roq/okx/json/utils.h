/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/okx/json/instrument_type.h"
#include "roq/okx/json/option_type.h"
#include "roq/okx/json/order_state.h"
#include "roq/okx/json/side.h"
#include "roq/okx/json/state.h"

namespace roq {
namespace okx {
namespace json {

template <typename T>
inline void update(T &result, const core::json::value_t &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(std::chrono::milliseconds &result, const core::json::value_t &value) {
  return std::visit(
      overloaded{
          [&](const core::json::null_t &) { result = std::chrono::milliseconds{}; },
          [](bool) { throw std::bad_cast(); },
          [&](int64_t value) { result = std::chrono::milliseconds{value}; },
          [&](double value) { result = std::chrono::milliseconds{static_cast<int64_t>(value)}; },
          [&](const std::string_view &value) {
            result =
                core::charconv::datetime_from_string<std::remove_reference<decltype(result)>::type>(
                    value);
          },
          [](const core::json::object_t &) { throw std::bad_cast(); },
          [](const core::json::array_t &) { throw std::bad_cast(); },
      },
      value);
}

template <>
inline void update(std::chrono::nanoseconds &result, const core::json::value_t &value) {
  return std::visit(
      overloaded{
          [&](const core::json::null_t &) { result = std::chrono::nanoseconds{}; },
          [](bool) { throw std::bad_cast(); },
          [&](int64_t value) { result = std::chrono::nanoseconds{value}; },
          [&](double value) { result = std::chrono::nanoseconds{static_cast<int64_t>(value)}; },
          [&](const std::string_view &value) {
            result =
                core::charconv::datetime_from_string<std::remove_reference<decltype(result)>::type>(
                    value);
          },
          [](const core::json::object_t &) { throw std::bad_cast(); },
          [](const core::json::array_t &) { throw std::bad_cast(); },
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
    case json::Side::UNDEFINED:
      break;
    case json::Side::UNKNOWN:
      break;
    case json::Side::BUY:
      return roq::Side::BUY;
    case json::Side::SELL:
      return roq::Side::SELL;
  }
  return {};
}

inline json::Side map(roq::Side value) {
  switch (value) {
    case roq::Side::UNDEFINED:
      break;
    case roq::Side::BUY:
      return json::Side::BUY;
    case roq::Side::SELL:
      return json::Side::SELL;
  }
  return {};
}

// instrument type

inline roq::SecurityType map(json::InstrumentType value) {
  switch (value) {
    case json::InstrumentType::UNDEFINED:
      break;
    case json::InstrumentType::UNKNOWN:
      break;
    case json::InstrumentType::SPOT:
      return roq::SecurityType::SPOT;
    case json::InstrumentType::MARGIN:
      return roq::SecurityType::SPOT;
    case json::InstrumentType::SWAP:
      return roq::SecurityType::SWAP;
    case json::InstrumentType::FUTURES:
      return roq::SecurityType::FUTURES;
    case json::InstrumentType::OPTION:
      return roq::SecurityType::OPTION;
  }
  return {};
}

// option type

inline roq::OptionType map(json::OptionType value) {
  switch (value) {
    case json::OptionType::UNDEFINED:
      break;
    case json::OptionType::UNKNOWN:
      break;
    case json::OptionType::CALL:
      return roq::OptionType::CALL;
    case json::OptionType::PUT:
      return roq::OptionType::PUT;
  }
  return {};
}

// state

inline roq::TradingStatus map(json::State value) {
  switch (value) {
    case json::State::UNDEFINED:
      break;
    case json::State::UNKNOWN:
      break;
    case json::State::LIVE:
      return roq::TradingStatus::OPEN;
    case json::State::SUSPENDED:
      return roq::TradingStatus::HALT;
    case json::State::PREOPEN:
      return roq::TradingStatus::PRE_OPEN;
    case json::State::SETTLEMENT:
      break;
  }
  return {};
}

// order state

inline roq::OrderStatus map(json::OrderState value) {
  switch (value) {
    case json::OrderState::UNDEFINED:
      break;
    case json::OrderState::UNKNOWN:
      break;
    case json::OrderState::CANCELED:
      return roq::OrderStatus::CANCELED;
    case json::OrderState::LIVE:
      return roq::OrderStatus::WORKING;
    case json::OrderState::PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case json::OrderState::FILLED:
      return roq::OrderStatus::COMPLETED;
  }
  return {};
}

/*
// ord type

inline roq::OrderType map(json::OrdType value) {
  switch (value) {
    case json::OrdType::UNDEFINED:
      break;
    case json::OrdType::UNKNOWN:
      break;
    case json::OrdType::MARKET:
      return roq::OrderType::MARKET;
    case json::OrdType::LIMIT:
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
