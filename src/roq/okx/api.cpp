/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/api.hpp"

#include "roq/exceptions.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === IMPLEMENTATION ===

API API::create(Settings const &) {
  return {
      .market_data{
          .instruments = "/api/v5/public/instruments"sv,
          .candles = "/api/v5/market/candles"sv,
      },
      .simple{
          .account_balance = "/api/v5/account/balance"sv,
          .account_positions = "/api/v5/account/positions"sv,
          .trade_orders_pending = "/api/v5/trade/orders-pending"sv,
          .trade_fills = "/api/v5/trade/fills"sv,
      },
  };
}

}  // namespace okx
}  // namespace roq
