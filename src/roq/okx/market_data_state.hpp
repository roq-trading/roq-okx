/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <cstdint>

namespace roq {
namespace okx {

enum class MarketDataState : uint8_t {
  UNDEFINED = 0,
  LOGIN,
  DONE,
};

}  // namespace okx
}  // namespace roq
