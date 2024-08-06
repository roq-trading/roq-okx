/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <tuple>

#include "roq/api.hpp"

#include "roq/okx/json/instrument_state.hpp"
#include "roq/okx/json/instrument_type.hpp"
#include "roq/okx/json/option_type.hpp"
#include "roq/okx/json/order_flow_type.hpp"
#include "roq/okx/json/order_state.hpp"
#include "roq/okx/json/side.hpp"

namespace roq {
namespace okx {
namespace json {

template <typename... Args>
struct Map final {
  explicit Map(Args &&...args) : args_{std::forward<Args>(args)...} {}
  explicit Map(Args const &...args) : args_{args...} {}

  Map(Map const &) = delete;

  template <typename R>
  operator R();

 private:
  std::tuple<Args...> const args_;
};

template <typename R, typename... Args>
inline R map(Args &&...args) {
  return static_cast<R>(Map{std::forward<Args>(args)...});
}

}  // namespace json
}  // namespace okx
}  // namespace roq
