/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/core/utility.h"

#include "roq/core/json/parser.h"

#include "roq/core/charconv/datetime.h"

#include "roq/okex/json/report_type.h"
#include "roq/okex/json/side.h"
#include "roq/okex/json/status.h"
#include "roq/okex/json/time_in_force.h"
#include "roq/okex/json/type.h"

namespace roq {
namespace okex {
namespace json {

template <typename T>
inline void update(T &result, const core::json::value_t &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(
    std::chrono::nanoseconds &result, const core::json::value_t &value) {
  result = std::chrono::milliseconds{core::json::get<uint64_t>(value)};
}

template <>
inline void update(ReportType &result, const core::json::value_t &value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(Side &result, const core::json::value_t &value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(Status &result, const core::json::value_t &value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(TimeInForce &result, const core::json::value_t &value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

template <>
inline void update(Type &result, const core::json::value_t &value) {
  using result_type = std::remove_reference<decltype(result)>::type;
  result = result_type(core::json::get<std::string_view>(value));
}

// ...

inline roq::Side map(json::Side side) {
  switch (side) {
    case json::Side::UNDEFINED:
      return roq::Side::UNDEFINED;
    case json::Side::BUY:
      return roq::Side::BUY;
    case json::Side::SELL:
      return roq::Side::SELL;
    default:
      return roq::Side::UNDEFINED;  // XXX UNKNOWN ??
  }
}

}  // namespace json
}  // namespace okex
}  // namespace roq
