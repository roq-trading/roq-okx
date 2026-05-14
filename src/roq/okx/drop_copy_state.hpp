/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace okx {

enum class DropCopyState : uint8_t {
  UNDEFINED = 0,
  LOGIN,
  SUBSCRIBE,
  BALANCE,
  POSITIONS,
  ORDERS,
  DONE,
};

}  // namespace okx
}  // namespace roq
