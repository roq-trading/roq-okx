/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.hpp"

#include "roq/core/charconv.hpp"
#include "roq/core/clock.hpp"
#include "roq/core/utils.hpp"

#include "roq/core/io/event_context.hpp"

#include "roq/okx/flags.hpp"

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

namespace {
Security NO_SECURITY;

template <typename R>
auto create_security(Config const &config) {
  R result;
  for (auto &[_, iter] : config.accounts)
    result.try_emplace(iter.name, std::make_unique<Security>(config, iter.name));
  return result;
}

auto &get_security(auto &security_by_account, auto const &master_account) {
  if (std::empty(security_by_account) && std::empty(master_account))
    return NO_SECURITY;
  assert(!std::empty(master_account));
  auto iter = security_by_account.find(master_account);
  if (iter == std::end(security_by_account))
    log::fatal("Unexpected"sv);
  return *(*iter).second;
}

template <typename R>
auto create_request(Config const &config) {
  R result;
  for (auto &[_, iter] : config.accounts)
    result.try_emplace(iter.name, Request{});
  return result;
}

template <typename R>
auto create_rest(
    Gateway &gateway,
    core::io::Context &context,
    uint16_t &stream_id,
    auto &security_by_account,
    Shared &shared,
    auto &request_by_account) {
  R result;
  for (auto &[account, security] : security_by_account) {
    auto &request = request_by_account[account];
    result.try_emplace(account, std::make_unique<Rest>(gateway, context, ++stream_id, *security, shared, request));
  }
  return result;
}

template <typename R>
auto create_order_entry(
    Gateway &gateway,
    core::io::Context &context,
    uint16_t &stream_id,
    auto &security_by_account,
    Shared &shared,
    auto &request_by_account) {
  R result;
  for (auto &[account, security] : security_by_account) {
    auto &request = request_by_account[account];
    result.try_emplace(
        account, std::make_unique<OrderEntry>(gateway, context, ++stream_id, *security, shared, request));
  }
  return result;
}

template <typename R>
auto create_market_data(
    Gateway &gateway,
    core::io::Context &context,
    uint16_t &stream_id,
    auto &security_by_account,
    auto &master_account,
    Shared &shared) {
  R result;
  ++stream_id;
  auto index = std::size(result);
  log::debug("Create MarketData (stream_id={}, index={})"sv, stream_id, index);
  auto market_data = std::make_unique<MarketData>(
      gateway, context, stream_id, get_security(security_by_account, master_account), shared, index);
  result.emplace_back(std::move(market_data));
  return result;
}
}  // namespace

Gateway::Gateway(server::Dispatcher &dispatcher, Config const &config)
    : dispatcher_(dispatcher), master_account_(config.get_master_account()),
      security_(create_security<decltype(security_)>(config)), context_(core::io::EventContext::create()),
      shared_(dispatcher), request_(create_request<decltype(request_)>(config)),
      rest_(create_rest<decltype(rest_)>(*this, *context_, ++stream_id_, security_, shared_, request_)),
      order_entry_(
          create_order_entry<decltype(order_entry_)>(*this, *context_, stream_id_, security_, shared_, request_)),
      market_data_(create_market_data<decltype(market_data_)>(
          *this, *context_, stream_id_, security_, master_account_, shared_)) {
}

void Gateway::operator()(Event<Start> const &event) {
  log::info("Starting the gateway..."sv);
  for (auto &[_, rest] : rest_)
    (*rest)(event);
  for (auto &[_, order_entry] : order_entry_)
    if (static_cast<bool>(order_entry))
      (*order_entry)(event);
  for (auto &market_data : market_data_)
    (*market_data)(event);
}

void Gateway::operator()(Event<Stop> const &event) {
  log::info("Stopping the gateway..."sv);
  for (auto &market_data : market_data_)
    (*market_data)(event);
  for (auto &[_, order_entry] : order_entry_)
    if (static_cast<bool>(order_entry))
      (*order_entry)(event);
  for (auto &[_, rest] : rest_)
    (*rest)(event);
}

void Gateway::operator()(Event<Timer> const &event) {
  for (auto &[_, rest] : rest_)
    (*rest)(event);
  for (auto &[_, order_entry] : order_entry_)
    if (static_cast<bool>(order_entry))
      (*order_entry)(event);
  for (auto &market_data : market_data_)
    (*market_data)(event);
  (*context_).drain();
}

void Gateway::operator()(Event<Connected> const &) {
}

void Gateway::operator()(Event<Disconnected> const &event) {
  auto const &[message_info, disconnected] = event;
  log::warn(
      R"(Disconnected: source="{}", order_cancel_policy={})"sv,
      message_info.source_name,
      disconnected.order_cancel_policy);
  switch (disconnected.order_cancel_policy) {
    using enum OrderCancelPolicy;
    case UNDEFINED:
      break;
    case MANAGED_ORDERS:
      log::warn("*** CANCEL MANAGED ORDERS NOT IMPLEMENTED ***"sv);
      break;
    case BY_ACCOUNT:
      log::warn("*** CANCEL ALL ACCOUNT ORDERS ***"sv);
      for (auto &[account, order_entry] : order_entry_) {
        if (dispatcher_.can_user_trade_account(account, message_info.source)) {
          log::warn(R"(- account="{}")"sv, account);
          CancelAllOrders cancel_all_orders{
              .account = account,
          };
          Event event(message_info, cancel_all_orders);
          (*order_entry)(event, {});
        }
      }
  }
}

void Gateway::operator()(Trace<StreamStatus const> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ExternalLatency const> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ReferenceData const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketStatus const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<TopOfBook const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketByPriceUpdate const> const &event, bool is_last, bool refresh) {
  dispatcher_(
      event, is_last, refresh, shared_.final_bids, shared_.final_asks, []([[maybe_unused]] auto &market_by_price) {});
}

void Gateway::operator()(Trace<TradeSummary const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<StatisticsUpdate const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<TradeUpdate const> const &event, bool is_last, uint8_t user_id) {
  dispatcher_(event, is_last, user_id);
}

void Gateway::operator()(Trace<FundsUpdate const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<PositionUpdate const> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(MarketData::SymbolsUpdate &symbols_update) {
  auto [size, start_from] = shared_.symbols(symbols_update.symbols);
  ensure_symbol_slices(size);
  for (auto &iter : market_data_)
    (*iter).subscribe(start_from);
}

void Gateway::ensure_symbol_slices(size_t size) {
  while (std::size(market_data_) < size) {
    auto stream_id = ++stream_id_;
    auto index = std::size(market_data_);
    log::debug("Create MarketData (stream_id={}, index={})"sv, stream_id, index);
    auto market_data = std::make_unique<MarketData>(
        *this, *context_, stream_id, get_security(security_, master_account_), shared_, index);
    MessageInfo message_info;
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    market_data_.emplace_back(std::move(market_data));
  }
}

uint16_t Gateway::operator()(
    Event<CreateOrder> const &event, oms::Order const &order, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  return get_order_entry(event.value.account)(event, order, request_id);
}

uint16_t Gateway::operator()(
    Event<ModifyOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    Event<CancelOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_order_entry(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(Event<CancelAllOrders> const &event, std::string_view const &request_id) {
  assert(!std::empty(event.value.account));
  return get_order_entry(event.value.account)(event, request_id);
}

void Gateway::operator()(metrics::Writer &writer) {
  for (auto &iter : rest_)
    (*iter.second)(writer);
  for (auto &[_, order_entry] : order_entry_)
    if (static_cast<bool>(order_entry))
      (*order_entry)(writer);
  for (auto &iter : market_data_)
    (*iter)(writer);
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account) {
  auto iter = order_entry_.find(account);
  if (iter != std::end(order_entry_))
    return *(*iter).second;
  throw RuntimeError(R"(Unknown account="{}")"sv, account);
}

}  // namespace okx
}  // namespace roq
