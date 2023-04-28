/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/gateway.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.hpp"

#include "roq/clock.hpp"
#include "roq/core/charconv.hpp"
#include "roq/core/utils.hpp"

#include "roq/okx/flags.hpp"

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
Account NO_SECURITY;  // XXX not const...
}

// === HELPERS ===

namespace {
template <typename R>
R create_accounts(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(account.name, std::make_unique<Account>(config, account.name));
  return result;
}

auto &get_account(auto &accounts, auto const &master_account) {
  if (std::empty(accounts) && std::empty(master_account))
    return NO_SECURITY;
  assert(!std::empty(master_account));
  auto iter = accounts.find(master_account);
  if (iter == std::end(accounts))
    log::fatal("Unexpected"sv);
  return *(*iter).second;
}

template <typename R>
R create_request(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, account] : config.accounts)
    result.try_emplace(account.name, Request{});
  return result;
}

template <typename R>
R create_rest(auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request_by_account) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[name, account] : accounts) {
    auto &request = request_by_account[name];
    result.try_emplace(name, std::make_unique<Rest>(gateway, context, ++stream_id, *account, shared, request));
  }
  return result;
}

template <typename R>
R create_order_entry(
    auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &shared, auto &request_by_account) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[name, account] : accounts) {
    auto &request = request_by_account[name];
    result.try_emplace(name, std::make_unique<OrderEntry>(gateway, context, ++stream_id, *account, shared, request));
  }
  return result;
}

template <typename R>
R create_market_data(
    auto &gateway, auto &context, auto &stream_id, auto &accounts, auto &master_account, auto &shared) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  ++stream_id;
  auto index = std::size(result);
  log::debug("Create MarketData (stream_id={}, index={})"sv, stream_id, index);
  auto market_data =
      std::make_unique<MarketData>(gateway, context, stream_id, get_account(accounts, master_account), shared, index);
  result.emplace_back(std::move(market_data));
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Gateway::Gateway(server::Dispatcher &dispatcher, Config const &config, io::Context &context)
    : dispatcher_{dispatcher}, accounts_{create_accounts<decltype(accounts_)>(config)},
      master_account_{config.get_master_account()}, context_{context}, shared_{dispatcher},
      request_{create_request<decltype(request_)>(config)},
      rest_{create_rest<decltype(rest_)>(*this, context_, ++stream_id_, accounts_, shared_, request_)},
      order_entry_{
          create_order_entry<decltype(order_entry_)>(*this, context_, stream_id_, accounts_, shared_, request_)},
      market_data_{create_market_data<decltype(market_data_)>(
          *this, context_, stream_id_, accounts_, master_account_, shared_)} {
}

void Gateway::operator()(Event<Start> const &event) {
  log::info("Starting..."sv);
  dispatch(event);
}

void Gateway::operator()(Event<Stop> const &event) {
  log::info("Stopping..."sv);
  dispatch(event);
}

void Gateway::operator()(Event<Timer> const &event) {
  dispatch(event);
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
          auto cancel_all_orders = CancelAllOrders{
              .account = account,
          };
          Event event{message_info, cancel_all_orders};
          (*order_entry)(event, {});
        }
      }
  }
}

void Gateway::operator()(Trace<StreamStatus> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ExternalLatency> const &event) {
  dispatcher_(event);
}

void Gateway::operator()(Trace<ReferenceData> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketStatus> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<TopOfBook> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<MarketByPriceUpdate> const &event, bool is_last) {
  auto callback = []([[maybe_unused]] auto &market_by_price) {};
  dispatcher_(event, is_last, bids_, asks_, callback);
}

void Gateway::operator()(Trace<TradeSummary> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<StatisticsUpdate> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<FundsUpdate> const &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(Trace<PositionUpdate> const &event, bool is_last) {
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
        *this, context_, stream_id, get_account(accounts_, master_account_), shared_, index);
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
  dispatch(writer);
}

template <typename... Args>
void Gateway::dispatch(Args &&...args) {
  auto helper = [&](auto &target) { target(std::forward<Args>(args)...); };
  for (auto &[_, item] : rest_)
    helper(*item);
  for (auto &[_, item] : order_entry_)
    if (static_cast<bool>(item))
      helper(*item);
  for (auto &item : market_data_)
    helper(*item);
}

OrderEntry &Gateway::get_order_entry(std::string_view const &account) {
  auto iter = order_entry_.find(account);
  if (iter != std::end(order_entry_))
    return *(*iter).second;
  throw RuntimeError{R"(Unknown account="{}")"sv, account};
}

}  // namespace okx
}  // namespace roq
