/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <chrono>

namespace roq {
namespace okx {

struct Request final {
  // balance
  std::chrono::nanoseconds request_balance = {};
  std::chrono::nanoseconds respond_balance = {};
  // position
  std::chrono::nanoseconds request_positions = {};
  std::chrono::nanoseconds respond_positions = {};
  // orders
  std::chrono::nanoseconds request_orders = {};
  std::chrono::nanoseconds respond_orders = {};
};

}  // namespace okx
}  // namespace roq
