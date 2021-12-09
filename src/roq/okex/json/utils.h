/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/okex/json/liquidity.h"
#include "roq/okex/json/order_status.h"
#include "roq/okex/json/order_type.h"
#include "roq/okex/json/side.h"
#include "roq/okex/json/time_in_force.h"

namespace roq {
namespace okex {
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

// order-type

inline roq::OrderType map(json::OrderType value) {
  switch (value) {
    case json::OrderType::UNDEFINED:
      break;
    case json::OrderType::UNKNOWN:
      break;
    case json::OrderType::LIMIT:
      return roq::OrderType::LIMIT;
    case json::OrderType::MARKET:
      return roq::OrderType::MARKET;
    case json::OrderType::STOP_LIMIT:
      break;
  }
  return {};
}

// order-status

inline roq::OrderStatus map(json::OrderStatus value) {
  switch (value) {
    case json::OrderStatus::UNDEFINED:
      break;
    case json::OrderStatus::UNKNOWN:
      break;
    case json::OrderStatus::OPEN:
      return roq::OrderStatus::WORKING;
    case json::OrderStatus::MATCH:
      return roq::OrderStatus::WORKING;  // ???
    case json::OrderStatus::DONE:
      return roq::OrderStatus::COMPLETED;
  }
  return {};
}

// order-type

inline json::OrderType map(roq::OrderType value) {
  switch (value) {
    case roq::OrderType::UNDEFINED:
      break;
    case roq::OrderType::MARKET:
      return json::OrderType::MARKET;
    case roq::OrderType::LIMIT:
      return json::OrderType::LIMIT;
  }
  return {};
}

// time-in-force

inline json::TimeInForce map(roq::TimeInForce value) {
  switch (value) {
    case roq::TimeInForce::UNDEFINED:
      break;
    case roq::TimeInForce::GFD:
      break;
    case roq::TimeInForce::GTC:
      return json::TimeInForce::GTC;
    case roq::TimeInForce::OPG:
      break;
    case roq::TimeInForce::IOC:
      return json::TimeInForce::IOC;
    case roq::TimeInForce::FOK:
      return json::TimeInForce::FOK;
    case roq::TimeInForce::GTX:
      break;
    case roq::TimeInForce::GTD:
      break;
    case roq::TimeInForce::AT_THE_CLOSE:
      break;
    case roq::TimeInForce::GOOD_THROUGH_CROSSING:
      break;
    case roq::TimeInForce::AT_CROSSING:
      break;
    case roq::TimeInForce::GOOD_FOR_TIME:
      break;
    case roq::TimeInForce::GFA:
      break;
    case roq::TimeInForce::GFM:
      break;
  }
  return {};
}

inline roq::TimeInForce map(json::TimeInForce value) {
  switch (value) {
    case json::TimeInForce::UNDEFINED:
      break;
    case json::TimeInForce::UNKNOWN:
      break;
    case json::TimeInForce::GTC:
      return roq::TimeInForce::GTC;
    case json::TimeInForce::GTT:
      break;  // don't know how to map
    case json::TimeInForce::IOC:
      return roq::TimeInForce::IOC;
    case json::TimeInForce::FOK:
      return roq::TimeInForce::FOK;
  }
  return {};
}

// liquidity

inline roq::Liquidity map(json::Liquidity value) {
  switch (value) {
    case json::Liquidity::UNDEFINED:
      break;
    case json::Liquidity::UNKNOWN:
      break;
    case json::Liquidity::MAKER:
      return roq::Liquidity::MAKER;
    case json::Liquidity::TAKER:
      return roq::Liquidity::TAKER;
  }
  return {};
}

// error

extern roq::Error guess_error(int32_t code);

}  // namespace json
}  // namespace okex
}  // namespace roq
