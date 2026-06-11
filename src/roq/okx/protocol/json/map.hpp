/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/okx/protocol/json/instrument_state.hpp"
#include "roq/okx/protocol/json/instrument_type.hpp"
#include "roq/okx/protocol/json/option_type.hpp"
#include "roq/okx/protocol/json/order_flow_type.hpp"
#include "roq/okx/protocol/json/order_state.hpp"
#include "roq/okx/protocol/json/side.hpp"

#include "roq/liquidity.hpp"
#include "roq/option_type.hpp"
#include "roq/order_status.hpp"
#include "roq/security_type.hpp"
#include "roq/side.hpp"
#include "roq/trading_status.hpp"

#include "roq/map.hpp"

namespace roq {

template <>
template <>
std::optional<TradingStatus> Map<okx::protocol::json::InstrumentState>::helper() const;

template <>
template <>
std::optional<SecurityType> Map<okx::protocol::json::InstrumentType>::helper() const;

template <>
template <>
std::optional<OptionType> Map<okx::protocol::json::OptionType>::helper() const;

template <>
template <>
std::optional<Liquidity> Map<okx::protocol::json::OrderFlowType>::helper() const;

template <>
template <>
std::optional<OrderStatus> Map<okx::protocol::json::OrderState>::helper() const;

template <>
template <>
std::optional<Side> Map<okx::protocol::json::Side>::helper() const;

// ===

template <>
template <>
std::optional<okx::protocol::json::Side> Map<Side>::helper() const;

}  // namespace roq
