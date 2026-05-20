/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <chrono>

namespace roq {
namespace okx {
namespace gateway {

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

}  // namespace gateway
}  // namespace okx
}  // namespace roq
