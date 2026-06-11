/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/protocol/json/encoder.hpp"

#include "roq/logging.hpp"

#include "roq/decimal.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/okx/protocol/json/map.hpp"
#include "roq/okx/protocol/json/order_type.hpp"
#include "roq/okx/protocol/json/position_side.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace protocol {
namespace json {

// === HELPERS ===

namespace {
std::pair<protocol::json::OrderType, bool> compute_order_attributes(auto order_type, auto time_in_force, auto execution_instructions) {
  auto log_no_mapping_exists = [&]() {
    log::error("No mapping exists for order_type={}, time_in_force={}, execution_instructions={}"sv, order_type, time_in_force, execution_instructions);
  };
  bool reduce_only = false;
  protocol::json::OrderType order_type_ = {};
  if (!std::empty(execution_instructions)) {
    if (execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE)) {
      order_type_ = protocol::json::OrderType::POST_ONLY;
    }
    if (execution_instructions.has(ExecutionInstruction::DO_NOT_INCREASE)) {
      reduce_only = true;
    }
    // throw server::oms::NotSupported{"not supported"sv};
  }
  switch (time_in_force) {
    using enum TimeInForce;
    case UNDEFINED:
    case GTC:
      break;
    case FOK:
      if (order_type_ != protocol::json::OrderType{}) {
        order_type_ = protocol::json::OrderType::FOK;
      }
      break;
    case IOC:
      if (order_type_ == protocol::json::OrderType{}) {
        order_type_ = protocol::json::OrderType::IOC;
      }
      break;
    default:
      log_no_mapping_exists();
      throw server::oms::NotSupported{"not supported"sv};
  }
  if (order_type_ == protocol::json::OrderType{}) {
    switch (order_type) {
      using enum roq::OrderType;
      case MARKET:
        order_type_ = protocol::json::OrderType::MARKET;
        break;
      case LIMIT:
        order_type_ = protocol::json::OrderType::LIMIT;
        break;
      default:
        log_no_mapping_exists();
        throw server::oms::NotSupported{"not supported"sv};
    }
  }
  return {order_type_, reduce_only};
}
}  // namespace

// === IMPLEMENTATION ===

std::string_view Encoder::batch_orders(
    std::string &buffer,
    CreateOrder const &create_order,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    uint64_t &request_id_2,
    TradeMode trade_mode,
    std::string_view const &price_amend_type,
    std::string_view const &margin_currency) {
  buffer.clear();
  protocol::json::PositionSide position_side = protocol::json::PositionSide::NET;  // XXX should be configurable
  auto side = map(create_order.side).template get<protocol::json::Side>();
  auto [order_type, reduce_only] = compute_order_attributes(create_order.order_type, create_order.time_in_force, create_order.execution_instructions);
  auto trade_mode_2 = [&]() -> protocol::json::TradeMode {
    switch (create_order.margin_mode) {
      using enum MarginMode;
      case UNDEFINED:
        break;
      case ISOLATED:
        return protocol::json::TradeMode::type_t::ISOLATED;
      case CROSS:
        return protocol::json::TradeMode::type_t::CROSS;
      case PORTFOLIO:
        throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_ARGS, "margin_mode"sv};
    }
    return trade_mode;
  }();
  std::string extras;
  if (trade_mode_2 == protocol::json::TradeMode::type_t::CROSS && !std::empty(margin_currency)) {
    extras = fmt::format(R"(,"ccy":"{}")"sv, margin_currency);
  }
  switch (order_type) {
    using enum protocol::json::OrderType::type_t;
    case MARKET: {
      if (order.security_type == SecurityType::SPOT) {
        fmt::format_to(
            std::back_inserter(buffer),
            R"({{)"
            R"("id":"{}",)"
            R"("op":"batch-orders",)"
            R"("args":[{{)"
            R"("clOrdId":"{}",)"
            R"("tdMode":"{}",)"
            R"("posSide":"{}",)"
            R"("instIdCode":{},)"
            R"("side":"{}",)"
            R"("ordType":"{}",)"
            R"("reduceOnly":{},)"
            R"("tgtCcy":"base_ccy",)"  // note!
            R"("sz":"{}")"
            R"({})"  // extras
            R"(}})"
            R"(])"
            R"(}})"sv,
            ++request_id_2,
            request_id,
            trade_mode_2.as_raw_text(),
            position_side.as_raw_text(),
            ref_data.external_security_id,
            side.as_raw_text(),
            order_type.as_raw_text(),
            reduce_only,
            Decimal{create_order.quantity, ref_data.quantity.precision},
            extras);
      } else {
        fmt::format_to(
            std::back_inserter(buffer),
            R"({{)"
            R"("id":"{}",)"
            R"("op":"batch-orders",)"
            R"("args":[{{)"
            R"("clOrdId":"{}",)"
            R"("tdMode":"{}",)"
            R"("posSide":"{}",)"
            R"("instIdCode":{},)"
            R"("side":"{}",)"
            R"("ordType":"{}",)"
            R"("reduceOnly":{},)"
            R"("sz":"{}")"
            R"({})"  // extras
            R"(}})"
            R"(])"
            R"(}})"sv,
            ++request_id_2,
            request_id,
            trade_mode_2.as_raw_text(),
            position_side.as_raw_text(),
            ref_data.external_security_id,
            side.as_raw_text(),
            order_type.as_raw_text(),
            reduce_only,
            Decimal{create_order.quantity, ref_data.quantity.precision},
            extras);
      }
      break;
    }
    default: {
      fmt::format_to(
          std::back_inserter(buffer),
          R"({{)"
          R"("id":"{}",)"
          R"("op":"batch-orders",)"
          R"("args":[{{)"
          R"("clOrdId":"{}",)"
          R"("tdMode":"{}",)"
          R"("posSide":"{}",)"
          R"("instIdCode":{},)"
          R"("side":"{}",)"
          R"("ordType":"{}",)"
          R"("reduceOnly":{},)"
          R"("sz":"{}",)"
          R"("px":"{}",)"
          R"("pxAmendType":"{}")"
          R"({})"  // extras
          R"(}})"
          R"(])"
          R"(}})"sv,
          ++request_id_2,
          request_id,
          trade_mode_2.as_raw_text(),
          position_side.as_raw_text(),
          ref_data.external_security_id,
          side.as_raw_text(),
          order_type.as_raw_text(),
          reduce_only,
          Decimal{create_order.quantity, ref_data.quantity.precision},
          Decimal{create_order.price, ref_data.price.precision},
          price_amend_type,
          extras);
    }
  }
  return buffer;
}

std::string_view Encoder::batch_amend_orders(
    std::string &buffer,
    ModifyOrder const &modify_order,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id,
    uint64_t &request_id_2,
    std::string_view const &price_amend_type) {
  buffer.clear();
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id = has_external_order_id ? std::string_view{order.external_order_id} : std::string_view{order.client_order_id};
  auto new_sz = std::isnan(modify_order.quantity) ? order.quantity : modify_order.quantity;
  auto new_px = std::isnan(modify_order.price) ? order.price : modify_order.price;
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("id":"{}",)"
      R"("op":"batch-amend-orders",)"
      R"("args":[{{)"
      R"("{}":"{}",)"
      R"("instIdCode":{},)"
      R"("reqId":"{}",)"
      R"("newSz":"{}",)"
      R"("newPx":"{}",)"
      R"("pxAmendType":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      ++request_id_2,
      order_id_type,
      order_id,
      ref_data.external_security_id,
      request_id,
      Decimal{new_sz, ref_data.quantity.precision},
      Decimal{new_px, ref_data.price.precision},
      price_amend_type);
  return buffer;
}

std::string_view Encoder::batch_cancel_orders(
    std::string &buffer,
    roq::CancelOrder const &,
    server::oms::Order const &order,
    server::oms::RefData const &ref_data,
    [[maybe_unused]] std::string_view const &request_id,
    [[maybe_unused]] std::string_view const &previous_request_id,
    uint64_t &request_id_2) {
  buffer.clear();
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id = has_external_order_id ? std::string_view{order.external_order_id} : std::string_view{order.client_order_id};
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("id":"{}",)"
      R"("op":"batch-cancel-orders",)"
      R"("args":[{{)"
      R"("{}":"{}",)"
      R"("instIdCode":{})"
      R"(}})"
      R"(])"
      R"(}})"sv,
      ++request_id_2,
      order_id_type,
      order_id,
      ref_data.external_security_id);
  return buffer;
}

std::string_view Encoder::batch_cancel_orders(
    std::string &buffer,
    CancelAllOrders const &,
    [[maybe_unused]] std::string_view const &request_id,
    uint64_t &request_id_2,
    std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_id) {
  fmt::format_to(
      std::back_inserter(buffer),
      R"({{)"
      R"("id":"{}",)"
      R"("op":"batch-cancel-orders",)"
      R"("args":[)"sv,
      ++request_id_2);
  auto first = true;
  for (auto &[symbol, external_order_id] : symbol_and_external_order_id) {
    if (!first) {
      fmt::format_to(std::back_inserter(buffer), ","sv);
    } else {
      first = false;
    }
    fmt::format_to(
        std::back_inserter(buffer),
        R"({{)"
        R"("instId":"{}",)"
        R"("ordId":"{}")"
        R"(}})",
        symbol,
        external_order_id);
  }
  fmt::format_to(
      std::back_inserter(buffer),
      R"(])"
      R"(}})"sv);
  return buffer;
}

}  // namespace json
}  // namespace protocol
}  // namespace okx
}  // namespace roq
