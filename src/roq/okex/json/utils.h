/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/okex/json/side.h"

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

}  // namespace json
}  // namespace okex
}  // namespace roq
