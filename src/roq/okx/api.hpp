/* Copyright (c) 2017-2025, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/okx/settings.hpp"

namespace roq {
namespace okx {

struct API final {
  struct {
    std::string_view instruments;
  } market_data;

  struct {
    std::string_view account_balance;
    std::string_view account_positions;
    std::string_view trade_orders_pending;
    std::string_view trade_fills;
  } simple;

  // factory
  static API create(Settings const &);
};

}  // namespace okx
}  // namespace roq
