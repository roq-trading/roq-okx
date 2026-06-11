/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/protocol/json/map.hpp"

using namespace std::literals;

namespace roq {

namespace {
template <typename... Args>
using Helper = detail::MapHelper<Args...>;
}

// okx => roq

// okx::protocol::json::InstrumentState ==> roq::SecurityType

template <>
template <>
constexpr Helper<okx::protocol::json::InstrumentState>::operator std::optional<roq::TradingStatus>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::InstrumentState::type_t;
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
    case POST_ONLY:
      return roq::TradingStatus::UNDEFINED;
    case REBASE:
      return roq::TradingStatus::UNDEFINED;
  }
  return {};
}

static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::UNDEFINED_INTERNAL}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::LIVE}} == roq::TradingStatus::OPEN);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::SUSPENDED}} == roq::TradingStatus::HALT);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::PREOPEN}} == roq::TradingStatus::PRE_OPEN);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::SETTLEMENT}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::EXPIRED}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::TEST}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::POST_ONLY}} == roq::TradingStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentState{okx::protocol::json::InstrumentState::REBASE}} == roq::TradingStatus::UNDEFINED);

template <>
template <>
std::optional<roq::TradingStatus> Map<okx::protocol::json::InstrumentState>::helper() const {
  return Helper{args_};
}

// okx::protocol::json::InstrumentType ==> roq::SecurityType

template <>
template <>
constexpr Helper<okx::protocol::json::InstrumentType>::operator std::optional<roq::SecurityType>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::InstrumentType::type_t;
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

static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::UNDEFINED_INTERNAL}} == roq::SecurityType::UNDEFINED);
static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::SPOT}} == roq::SecurityType::SPOT);
static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::MARGIN}} == roq::SecurityType::SPOT);
static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::SWAP}} == roq::SecurityType::SWAP);
static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::FUTURES}} == roq::SecurityType::FUTURES);
static_assert(Helper{okx::protocol::json::InstrumentType{okx::protocol::json::InstrumentType::OPTION}} == roq::SecurityType::OPTION);

template <>
template <>
std::optional<roq::SecurityType> Map<okx::protocol::json::InstrumentType>::helper() const {
  return Helper{args_};
}

// okx::protocol::json::OptionType ==> roq::OptionType

template <>
template <>
constexpr Helper<okx::protocol::json::OptionType>::operator std::optional<roq::OptionType>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::OptionType::type_t;
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

static_assert(Helper{okx::protocol::json::OptionType{okx::protocol::json::OptionType::UNDEFINED_INTERNAL}} == roq::OptionType::UNDEFINED);
static_assert(Helper{okx::protocol::json::OptionType{okx::protocol::json::OptionType::CALL}} == roq::OptionType::CALL);
static_assert(Helper{okx::protocol::json::OptionType{okx::protocol::json::OptionType::PUT}} == roq::OptionType::PUT);

template <>
template <>
std::optional<roq::OptionType> Map<okx::protocol::json::OptionType>::helper() const {
  return Helper{args_};
}

// okx::protocol::json::OrderFlowType ==> roq::OrderStatus

template <>
template <>
constexpr Helper<okx::protocol::json::OrderFlowType>::operator std::optional<roq::Liquidity>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::OrderFlowType::type_t;
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

static_assert(Helper{okx::protocol::json::OrderFlowType{okx::protocol::json::OrderFlowType::UNDEFINED_INTERNAL}} == roq::Liquidity::UNDEFINED);
static_assert(Helper{okx::protocol::json::OrderFlowType{okx::protocol::json::OrderFlowType::MAKER}} == roq::Liquidity::MAKER);
static_assert(Helper{okx::protocol::json::OrderFlowType{okx::protocol::json::OrderFlowType::TAKER}} == roq::Liquidity::TAKER);

template <>
template <>
std::optional<roq::Liquidity> Map<okx::protocol::json::OrderFlowType>::helper() const {
  return Helper{args_};
}

// okx::protocol::json::OrderState ==> roq::OrderStatus

template <>
template <>
constexpr Helper<okx::protocol::json::OrderState>::operator std::optional<roq::OrderStatus>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::OrderState::type_t;
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

static_assert(Helper{okx::protocol::json::OrderState{okx::protocol::json::OrderState::UNDEFINED_INTERNAL}} == roq::OrderStatus::UNDEFINED);
static_assert(Helper{okx::protocol::json::OrderState{okx::protocol::json::OrderState::CANCELED}} == roq::OrderStatus::CANCELED);
static_assert(Helper{okx::protocol::json::OrderState{okx::protocol::json::OrderState::LIVE}} == roq::OrderStatus::WORKING);
static_assert(Helper{okx::protocol::json::OrderState{okx::protocol::json::OrderState::PARTIALLY_FILLED}} == roq::OrderStatus::WORKING);
static_assert(Helper{okx::protocol::json::OrderState{okx::protocol::json::OrderState::FILLED}} == roq::OrderStatus::COMPLETED);

template <>
template <>
std::optional<roq::OrderStatus> Map<okx::protocol::json::OrderState>::helper() const {
  return Helper{args_};
}

// okx::protocol::json::Side ==> roq::Side

template <>
template <>
constexpr Helper<okx::protocol::json::Side>::operator std::optional<roq::Side>() const {
  switch (std::get<0>(args_)) {
    using enum okx::protocol::json::Side::type_t;
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

static_assert(Helper{okx::protocol::json::Side{okx::protocol::json::Side::UNDEFINED_INTERNAL}} == roq::Side::UNDEFINED);
static_assert(Helper{okx::protocol::json::Side{okx::protocol::json::Side::BUY}} == roq::Side::BUY);
static_assert(Helper{okx::protocol::json::Side{okx::protocol::json::Side::SELL}} == roq::Side::SELL);

template <>
template <>
std::optional<roq::Side> Map<okx::protocol::json::Side>::helper() const {
  return Helper{args_};
}

// roq ==> okx::json

// roq::Side ==> okx::protocol::json::Side

template <>
template <>
constexpr Helper<roq::Side>::operator std::optional<okx::protocol::json::Side>() const {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return okx::protocol::json::Side::UNDEFINED_INTERNAL;
    case BUY:
      return okx::protocol::json::Side::BUY;
    case SELL:
      return okx::protocol::json::Side::SELL;
  }
  return {};
}

template <>
template <>
std::optional<okx::protocol::json::Side> Map<roq::Side>::helper() const {
  return Helper{args_};
}

}  // namespace roq
