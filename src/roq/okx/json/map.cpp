/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/json/map.hpp"

using namespace std::literals;

namespace roq {

namespace {
template <typename... Args>
using Helper = detail::MapHelper<Args...>;
}

// okx => roq

// okx::json::InstrumentState ==> roq::SecurityType

template <>
template <>
constexpr Helper<okx::json::InstrumentState>::operator std::optional<roq::TradingStatus>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::InstrumentState::type_t;
    case UNDEFINED_INTERNAL:
      return roq::TradingStatus::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::TradingStatus::UNDEFINED;
    case LIVE:
      return roq::TradingStatus::OPEN;
    case SUSPENDED:
      return roq::TradingStatus::HALT;
    case PREOPEN:
      return roq::TradingStatus::PRE_OPEN;
    case SETTLEMENT:
      return roq::TradingStatus::UNDEFINED;
    case EXPIRED:
      return roq::TradingStatus::UNDEFINED;
    case TEST:
      return roq::TradingStatus::UNDEFINED;
  }
  return {};
}

static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::UNDEFINED_INTERNAL}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::LIVE}} == roq::TradingStatus::OPEN);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::SUSPENDED}} == roq::TradingStatus::HALT);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::PREOPEN}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::SETTLEMENT}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::EXPIRED}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::json::InstrumentState{okx::json::InstrumentState::TEST}} == roq::TradingStatus::UNDEFINED);

template <>
template <>
std::optional<roq::TradingStatus> Map<okx::json::InstrumentState>::helper() const {
  return Helper{args_};
}

// okx::json::InstrumentType ==> roq::SecurityType

template <>
template <>
constexpr Helper<okx::json::InstrumentType>::operator std::optional<roq::SecurityType>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::InstrumentType::type_t;
    case UNDEFINED_INTERNAL:
      return roq::SecurityType::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::SecurityType::UNDEFINED;
    case SPOT:
      return roq::SecurityType::SPOT;
    case MARGIN:
      return roq::SecurityType::SPOT;
    case SWAP:
      return roq::SecurityType::SWAP;
    case FUTURES:
      return roq::SecurityType::FUTURES;
    case OPTION:
      return roq::SecurityType::OPTION;
  }
  return {};
}

static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::UNDEFINED_INTERNAL}} == roq::SecurityType::UNDEFINED);
static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::SPOT}} == roq::SecurityType::SPOT);
static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::MARGIN}} == roq::SecurityType::SPOT);
static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::SWAP}} == roq::SecurityType::SWAP);
static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::FUTURES}} == roq::SecurityType::FUTURES);
static_assert(Helper{okx::json::InstrumentType{okx::json::InstrumentType::OPTION}} == roq::SecurityType::OPTION);

template <>
template <>
std::optional<roq::SecurityType> Map<okx::json::InstrumentType>::helper() const {
  return Helper{args_};
}

// okx::json::OptionType ==> roq::OptionType

template <>
template <>
constexpr Helper<okx::json::OptionType>::operator std::optional<roq::OptionType>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::OptionType::type_t;
    case UNDEFINED_INTERNAL:
      return roq::OptionType::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::OptionType::UNDEFINED;
    case CALL:
      return roq::OptionType::CALL;
    case PUT:
      return roq::OptionType::PUT;
  }
  return {};
}

static_assert(Helper{okx::json::OptionType{okx::json::OptionType::UNDEFINED_INTERNAL}} == roq::OptionType::UNDEFINED);
static_assert(Helper{okx::json::OptionType{okx::json::OptionType::CALL}} == roq::OptionType::CALL);
static_assert(Helper{okx::json::OptionType{okx::json::OptionType::PUT}} == roq::OptionType::PUT);

template <>
template <>
std::optional<roq::OptionType> Map<okx::json::OptionType>::helper() const {
  return Helper{args_};
}

// okx::json::OrderFlowType ==> roq::OrderStatus

template <>
template <>
constexpr Helper<okx::json::OrderFlowType>::operator std::optional<roq::Liquidity>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::OrderFlowType::type_t;
    case UNDEFINED_INTERNAL:
      return roq::Liquidity::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::Liquidity::UNDEFINED;
    case MAKER:
      return roq::Liquidity::MAKER;
    case TAKER:
      return roq::Liquidity::TAKER;
  }
  return {};
}

static_assert(Helper{okx::json::OrderFlowType{okx::json::OrderFlowType::UNDEFINED_INTERNAL}} == roq::Liquidity::UNDEFINED);
static_assert(Helper{okx::json::OrderFlowType{okx::json::OrderFlowType::MAKER}} == roq::Liquidity::MAKER);
static_assert(Helper{okx::json::OrderFlowType{okx::json::OrderFlowType::TAKER}} == roq::Liquidity::TAKER);

template <>
template <>
std::optional<roq::Liquidity> Map<okx::json::OrderFlowType>::helper() const {
  return Helper{args_};
}

// okx::json::OrderState ==> roq::OrderStatus

template <>
template <>
constexpr Helper<okx::json::OrderState>::operator std::optional<roq::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::OrderState::type_t;
    case UNDEFINED_INTERNAL:
      return roq::OrderStatus::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::OrderStatus::UNDEFINED;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case LIVE:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
  }
  return {};
}

static_assert(Helper{okx::json::OrderState{okx::json::OrderState::UNDEFINED_INTERNAL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{okx::json::OrderState{okx::json::OrderState::CANCELED}} == roq::OrderStatus::CANCELED);
static_assert(Helper{okx::json::OrderState{okx::json::OrderState::LIVE}} == roq::OrderStatus::WORKING);
static_assert(Helper{okx::json::OrderState{okx::json::OrderState::PARTIALLY_FILLED}} == roq::OrderStatus::WORKING);
static_assert(Helper{okx::json::OrderState{okx::json::OrderState::FILLED}} == roq::OrderStatus::COMPLETED);

template <>
template <>
std::optional<roq::OrderStatus> Map<okx::json::OrderState>::helper() const {
  return Helper{args_};
}

// okx::json::Side ==> roq::Side

template <>
template <>
constexpr Helper<okx::json::Side>::operator std::optional<roq::Side>() const {
  switch (std::get<0>(args_)) {
    using enum okx::json::Side::type_t;
    case UNDEFINED_INTERNAL:
      return roq::Side::UNDEFINED;
    case UNKNOWN_INTERNAL:
      return roq::Side::UNDEFINED;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  return {};
}

static_assert(Helper{okx::json::Side{okx::json::Side::UNDEFINED_INTERNAL}} == roq::Side::UNDEFINED);
static_assert(Helper{okx::json::Side{okx::json::Side::BUY}} == roq::Side::BUY);
static_assert(Helper{okx::json::Side{okx::json::Side::SELL}} == roq::Side::SELL);

template <>
template <>
std::optional<roq::Side> Map<okx::json::Side>::helper() const {
  return Helper{args_};
}

// roq ==> okx::json

// roq::Side ==> okx::json::Side

template <>
template <>
constexpr Helper<roq::Side>::operator std::optional<okx::json::Side>() const {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return okx::json::Side::UNDEFINED_INTERNAL;
    case BUY:
      return okx::json::Side::BUY;
    case SELL:
      return okx::json::Side::SELL;
  }
  return {};
}

template <>
template <>
std::optional<okx::json::Side> Map<roq::Side>::helper() const {
  return Helper{args_};
}

}  // namespace roq
