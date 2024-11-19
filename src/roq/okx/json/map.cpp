/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/okx/json/map.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

// === HELPERS ===

namespace {
// note! constexpr helper for static testing
template <typename... Args>
struct Helper final {
  explicit constexpr Helper(std::tuple<Args...> const &args) : args_{args} {}
  explicit constexpr Helper(Args &&...args_) : args_{std::forward<Args>(args_)...} {}

  template <typename R>
  constexpr operator R();

 private:
  std::tuple<Args...> const args_;
};

// ==> roq

// InstrumentState ==> roq::SecurityType

template <>
template <>
constexpr Helper<InstrumentState>::operator roq::TradingStatus() {
  switch (std::get<0>(args_)) {
    using enum json::InstrumentState::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case LIVE:
      return roq::TradingStatus::OPEN;
    case SUSPENDED:
      return roq::TradingStatus::HALT;
    case PREOPEN:
      return roq::TradingStatus::PRE_OPEN;
    case SETTLEMENT:
      return {};  // note!
    case EXPIRED:
      return {};  // note!
    case TEST:
      return {};  // note!
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::UNDEFINED__}}) == roq::TradingStatus::UNDEFINED);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::LIVE}}) == roq::TradingStatus::OPEN);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::SUSPENDED}}) == roq::TradingStatus::HALT);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::PREOPEN}}) == roq::TradingStatus::PRE_OPEN);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::SETTLEMENT}}) == roq::TradingStatus::UNDEFINED);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::EXPIRED}}) == roq::TradingStatus::UNDEFINED);
static_assert(static_cast<roq::TradingStatus>(Helper{InstrumentState{InstrumentState::TEST}}) == roq::TradingStatus::UNDEFINED);

// InstrumentType ==> roq::SecurityType

template <>
template <>
constexpr Helper<InstrumentType>::operator roq::SecurityType() {
  switch (std::get<0>(args_)) {
    using enum json::InstrumentType::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
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
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::UNDEFINED__}}) == roq::SecurityType::UNDEFINED);
static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::SPOT}}) == roq::SecurityType::SPOT);
static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::MARGIN}}) == roq::SecurityType::SPOT);
static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::SWAP}}) == roq::SecurityType::SWAP);
static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::FUTURES}}) == roq::SecurityType::FUTURES);
static_assert(static_cast<roq::SecurityType>(Helper{InstrumentType{InstrumentType::OPTION}}) == roq::SecurityType::OPTION);

// OptionType ==> roq::OptionType

template <>
template <>
constexpr Helper<OptionType>::operator roq::OptionType() {
  switch (std::get<0>(args_)) {
    using enum json::OptionType::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case CALL:
      return roq::OptionType::CALL;
    case PUT:
      return roq::OptionType::PUT;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::OptionType>(Helper{OptionType{OptionType::UNDEFINED__}}) == roq::OptionType::UNDEFINED);
static_assert(static_cast<roq::OptionType>(Helper{OptionType{OptionType::CALL}}) == roq::OptionType::CALL);
static_assert(static_cast<roq::OptionType>(Helper{OptionType{OptionType::PUT}}) == roq::OptionType::PUT);

// OrderFlowType ==> roq::OrderStatus

template <>
template <>
constexpr Helper<OrderFlowType>::operator roq::Liquidity() {
  switch (std::get<0>(args_)) {
    using enum json::OrderFlowType::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case MAKER:
      return roq::Liquidity::MAKER;
    case TAKER:
      return roq::Liquidity::TAKER;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::Liquidity>(Helper{OrderFlowType{OrderFlowType::UNDEFINED__}}) == roq::Liquidity::UNDEFINED);
static_assert(static_cast<roq::Liquidity>(Helper{OrderFlowType{OrderFlowType::MAKER}}) == roq::Liquidity::MAKER);
static_assert(static_cast<roq::Liquidity>(Helper{OrderFlowType{OrderFlowType::TAKER}}) == roq::Liquidity::TAKER);

// OrderState ==> roq::OrderStatus

template <>
template <>
constexpr Helper<OrderState>::operator roq::OrderStatus() {
  switch (std::get<0>(args_)) {
    using enum json::OrderState::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case CANCELED:
      return roq::OrderStatus::CANCELED;
    case LIVE:
      return roq::OrderStatus::WORKING;
    case PARTIALLY_FILLED:
      return roq::OrderStatus::WORKING;
    case FILLED:
      return roq::OrderStatus::COMPLETED;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::OrderStatus>(Helper{OrderState{OrderState::UNDEFINED__}}) == roq::OrderStatus::UNDEFINED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderState{OrderState::CANCELED}}) == roq::OrderStatus::CANCELED);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderState{OrderState::LIVE}}) == roq::OrderStatus::WORKING);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderState{OrderState::PARTIALLY_FILLED}}) == roq::OrderStatus::WORKING);
static_assert(static_cast<roq::OrderStatus>(Helper{OrderState{OrderState::FILLED}}) == roq::OrderStatus::COMPLETED);

// Side ==> roq::Side

template <>
template <>
constexpr Helper<Side>::operator roq::Side() {
  switch (std::get<0>(args_)) {
    using enum json::Side::type_t;
    case UNDEFINED__:
      return {};
    case UNKNOWN__:
      break;
    case BUY:
      return roq::Side::BUY;
    case SELL:
      return roq::Side::SELL;
  }
  roq::log::fatal("Unexpected"sv);
}

static_assert(static_cast<roq::Side>(Helper{Side{Side::UNDEFINED__}}) == roq::Side::UNDEFINED);
static_assert(static_cast<roq::Side>(Helper{Side{Side::BUY}}) == roq::Side::BUY);
static_assert(static_cast<roq::Side>(Helper{Side{Side::SELL}}) == roq::Side::SELL);

// roq ==>

// roq::Side ==> Side

template <>
template <>
constexpr Helper<roq::Side>::operator Side() {
  switch (std::get<0>(args_)) {
    using enum roq::Side;
    case UNDEFINED:
      return {};
    case BUY:
      return json::Side::BUY;
    case SELL:
      return json::Side::SELL;
  }
  roq::log::fatal("Unexpected"sv);
}
}  // namespace

// === IMPLEMENTATION ===

// ==> roq

template <>
template <>
Map<InstrumentState>::operator roq::TradingStatus() {
  return Helper{args_};
}

template <>
template <>
Map<InstrumentType>::operator roq::SecurityType() {
  return Helper{args_};
}

template <>
template <>
Map<OptionType>::operator roq::OptionType() {
  return Helper{args_};
}

template <>
template <>
Map<OrderFlowType>::operator roq::Liquidity() {
  return Helper{args_};
}

template <>
template <>
Map<OrderState>::operator roq::OrderStatus() {
  return Helper{args_};
}

template <>
template <>
Map<Side>::operator roq::Side() {
  return Helper{args_};
}

// roq ==>

template <>
template <>
Map<roq::Side>::operator Side() {
  return Helper{args_};
}

}  // namespace json
}  // namespace okx
}  // namespace roq
