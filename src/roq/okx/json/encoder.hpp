/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include <string>

#include "roq/cancel_all_orders.hpp"
#include "roq/cancel_order.hpp"
#include "roq/create_order.hpp"
#include "roq/modify_order.hpp"

#include "roq/server/oms/order.hpp"

#include "roq/okx/json/trade_mode.hpp"

namespace roq {
namespace okx {
namespace json {

struct Encoder final {
  static std::string_view batch_orders(
      std::string &buffer,
      CreateOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      uint64_t &request_id_2,
      TradeMode,
      std::string_view const &margin_currency);

  static std::string_view batch_amend_orders(
      std::string &buffer,
      ModifyOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      uint64_t &request_id_2);

  static std::string_view batch_cancel_orders(
      std::string &buffer,
      roq::CancelOrder const &,
      server::oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id,
      uint64_t &request_id_2);

  static std::string_view batch_cancel_orders(
      std::string &buffer,
      CancelAllOrders const &,
      std::string_view const &request_id,
      uint64_t &request_id_2,
      std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_id);
};

}  // namespace json
}  // namespace okx
}  // namespace roq
