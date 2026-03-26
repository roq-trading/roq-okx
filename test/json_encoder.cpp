/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/encoder.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

// === HELPERS ===

namespace {
auto create_create_order(Side side, double quantity, double price) {
  auto create_order = CreateOrder{
      .account = "A1"sv,
      .order_id = 1234,
      .exchange = "okx"sv,
      .symbol = "ABC"sv,
      .side = side,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = NaN,
      .order_type = OrderType::LIMIT,
      .time_in_force = TimeInForce::GTC,
      .execution_instructions = {},
      .request_template = {},
      .quantity = quantity,
      .price = price,
      .stop_price = NaN,
      .leverage = NaN,
      .routing_id = {},
      .strategy_id = {},
      .release_time_utc = {},
  };
  return create_order;
}

auto create_modify_order(double quantity, double price) {
  auto modify_order = ModifyOrder{
      .account = "A1"sv,
      .order_id = 1234,
      .request_template = {},
      .quantity = quantity,
      .price = price,
      .routing_id = {},
      .version = {},
      .conditional_on_version = {},
      .release_time_utc = {},
  };
  return modify_order;
}

auto create_oms_order(double quantity, double price) {
  auto order = server::oms::Order{
      .user_id = {},
      .stream_id = {},
      .account = {},
      .order_id = {},
      .exchange = "binance"sv,
      .symbol = "BTC"sv,
      .side = Side::BUY,
      .position_effect = {},
      .margin_mode = {},
      .quantity_type = {},
      .max_show_quantity = {},
      .order_type = {},
      .time_in_force = {},
      .execution_instructions = {},
      .create_time_utc = {},
      .update_time_utc = {},
      .external_account = {},
      .external_order_id = "oid:1234"sv,
      .client_order_id = {},
      .order_status = {},
      .quantity = quantity,
      .price = price,
      .stop_price = {},
      .leverage = NaN,
      .risk_exposure = {},
      .remaining_quantity = {},
      .traded_quantity = {},
      .average_traded_price = {},
      .last_traded_price = {},
      .last_traded_quantity = {},
      .last_liquidity = {},
      .routing_id = {},
      .max_request_version = {},
      .max_response_version = {},
      .max_accepted_version = {},
      .security_type = {},
      /*
      .quantity_precision{
          .increment = 0.1,
          .precision = Precision::_1,
      },
      .price_precision{
          .increment = 0.01,
          .precision = Precision::_2,
      },
      */
      .update_type = {},
      .user = {},
      .strategy_id = {},
  };
  return order;
}

auto create_ref_data() {
  auto ref_data = server::oms::RefData{
      .security_type = {},
      .external_security_id = {},
      .multiplier = NaN,
      .quantity = {},
      .price = {},
      .has_tick_size_steps = false,
  };
  ref_data.quantity = {
      .increment = 0.1,
      .precision = Precision::_1,
  };
  ref_data.price = {
      .increment = 0.01,
      .precision = Precision::_2,
  };
  return ref_data;
}
}  // namespace

// === IMPLEMENTATION ===

TEST_CASE("create_order", "[json_encoder]") {
  std::string buffer;
  auto create_order = create_create_order(Side::BUY, 1.0, 1.0);
  auto order = create_oms_order(1.0, 1.0);
  auto ref_data = create_ref_data();
  uint64_t request_id = 0;
  auto result = json::Encoder::batch_orders(buffer, create_order, order, ref_data, "1234"sv, request_id, json::TradeMode::CROSS, "BTC"sv);
  CHECK(
      result == R"({)"
                R"("id":"1",)"
                R"("op":"batch-orders",)"
                R"("args":[{)"
                R"("clOrdId":"1234",)"
                R"("tdMode":"cross",)"
                R"("posSide":"net",)"
                R"("instIdCode":0,)"
                R"("side":"buy",)"
                R"("ordType":"limit",)"
                R"("reduceOnly":false,)"
                R"("sz":"1.0",)"
                R"("px":"1.00",)"
                R"("ccy":"BTC")"
                R"(})"
                R"(])"
                R"(})"sv);
}

TEST_CASE("modify_order", "[json_encoder]") {
  std::string buffer;
  auto modify_order = create_modify_order(1.0, 1.0);
  auto order = create_oms_order(1.0, 1.0);
  auto ref_data = create_ref_data();
  uint64_t request_id = 0;
  auto result = json::Encoder::batch_amend_orders(buffer, modify_order, order, ref_data, "1234"sv, "2345"sv, request_id);
  CHECK(
      result == R"({)"
                R"("id":"1",)"
                R"("op":"batch-amend-orders",)"
                R"("args":[{)"
                R"("ordId":"oid:1234",)"
                R"("instIdCode":0,)"
                R"("reqId":"1234",)"
                R"("newSz":"1.0",)"
                R"("newPx":"1.00")"
                R"(})"
                R"(])"
                R"(})"sv);
}

TEST_CASE("cancel_all_orders_1", "[json_encoder]") {
  std::string buffer;
  CancelAllOrders cancel_all_orders;
  uint64_t request_id = 0;
  std::vector<std::pair<std::string_view, std::string_view>> symbol_and_external_order_id{{
      {"BTC"sv, "order_id:1234"sv},
  }};
  auto result = json::Encoder::batch_cancel_orders(buffer, cancel_all_orders, "1234"sv, request_id, symbol_and_external_order_id);
  CHECK(
      result == R"({)"
                R"("id":"1",)"
                R"("op":"batch-cancel-orders",)"
                R"("args":[{)"
                R"("instId":"BTC",)"
                R"("ordId":"order_id:1234")"
                R"(})"
                R"(])"
                R"(})"sv);
}

TEST_CASE("cancel_all_orders_2", "[json_encoder]") {
  std::string buffer;
  CancelAllOrders cancel_all_orders;
  uint64_t request_id = 0;
  std::vector<std::pair<std::string_view, std::string_view>> symbol_and_external_order_id{{
      {"BTC"sv, "order_id:1234"sv},
      {"ETH"sv, "order_id:2345"sv},
  }};
  auto result = json::Encoder::batch_cancel_orders(buffer, cancel_all_orders, "1234"sv, request_id, symbol_and_external_order_id);
  CHECK(
      result == R"({)"
                R"("id":"1",)"
                R"("op":"batch-cancel-orders",)"
                R"("args":[{)"
                R"("instId":"BTC",)"
                R"("ordId":"order_id:1234")"
                R"(},{)"
                R"("instId":"ETH",)"
                R"("ordId":"order_id:2345")"
                R"(})"
                R"(])"
                R"(})"sv);
}
