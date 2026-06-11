/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <chrono>

#include "roq/utils/patterns.hpp"

#include "roq/utils/charconv/from_chars.hpp"

#include "roq/core/json/parser.hpp"

namespace roq {
namespace okx {
namespace protocol {
namespace json {

template <typename T>
inline void update(T &result, core::json::Value const &value) {
  result = core::json::get<T>(value);
}

template <>
inline void update(std::chrono::milliseconds &result, core::json::Value const &value) {
  using result_type = std::remove_cvref_t<decltype(result)>;
  std::visit(
      utils::overloaded{
          [&](core::json::Null const &) { result = result_type{}; },
          [](bool) { throw std::bad_cast{}; },
          [&](int64_t value) { result = result_type{value}; },
          [&](double value) { result = result_type{static_cast<int64_t>(value)}; },
          [&](std::string_view const &value) { result = utils::charconv::from_chars<result_type>(value, utils::charconv::Format::DATETIME); },
          [](core::json::Object const &) { throw std::bad_cast{}; },
          [](core::json::Array const &) { throw std::bad_cast{}; },
      },
      value);
}

template <>
inline void update(std::chrono::microseconds &result, core::json::Value const &value) {
  using result_type = std::remove_cvref_t<decltype(result)>;
  std::visit(
      utils::overloaded{
          [&](core::json::Null const &) { result = result_type{}; },
          [](bool) { throw std::bad_cast{}; },
          [&](int64_t value) { result = result_type{value}; },
          [&](double value) { result = result_type{static_cast<int64_t>(value)}; },
          [&](std::string_view const &value) { result = utils::charconv::from_chars<result_type>(value, utils::charconv::Format::DATETIME); },
          [](core::json::Object const &) { throw std::bad_cast{}; },
          [](core::json::Array const &) { throw std::bad_cast{}; },
      },
      value);
}

template <>
inline void update(std::chrono::nanoseconds &result, core::json::Value const &value) {
  using result_type = std::remove_cvref_t<decltype(result)>;
  std::visit(
      utils::overloaded{
          [&](core::json::Null const &) { result = result_type{}; },
          [](bool) { throw std::bad_cast{}; },
          [&](int64_t value) { result = result_type{value}; },
          [&](double value) { result = result_type{static_cast<int64_t>(value)}; },
          [&](std::string_view const &value) { result = utils::charconv::from_chars<result_type>(value, utils::charconv::Format::DATETIME); },
          [](core::json::Object const &) { throw std::bad_cast{}; },
          [](core::json::Array const &) { throw std::bad_cast{}; },
      },
      value);
}

inline std::string_view strip_symbol_from_topic(std::string_view const &topic) {
  auto pos = topic.find_last_of(':');
  return pos == std::string_view::npos ? topic : topic.substr(pos + 1);
}

// error

extern roq::Error guess_error(int32_t code);

}  // namespace json
}  // namespace protocol
}  // namespace okx
}  // namespace roq
