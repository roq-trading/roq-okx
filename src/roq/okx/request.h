/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <chrono>

namespace roq {
namespace okx {

struct Request final {
  std::chrono::nanoseconds request_orders = {};
  std::chrono::nanoseconds respond_orders = {};
};

}  // namespace okx
}  // namespace roq
