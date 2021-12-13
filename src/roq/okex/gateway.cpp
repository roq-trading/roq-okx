/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/gateway.h"

#include <algorithm>
#include <cctype>
#include <limits>

#include "roq/logging.h"

#include "roq/core/charconv.h"
#include "roq/core/clock.h"
#include "roq/core/utils.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static auto create_security(const Config &config) {
  absl::flat_hash_map<std::string, std::unique_ptr<Security>> result;
  for (auto &[_, iter] : config.accounts) {
    result.try_emplace(iter.name, std::make_unique<Security>(config, iter.name));
  }
  return result;
}

template <typename T>
static auto create_drop_copy(
    Gateway &gateway,
    core::io::Context &context,
    uint16_t &stream_id,
    T &security,
    Shared &shared) {
  absl::flat_hash_map<std::string, std::unique_ptr<DropCopy>> result;
  for (auto &iter : security) {
    result.try_emplace(
        iter.first,
        std::make_unique<DropCopy>(gateway, context, ++stream_id, *iter.second, shared));
  }
  return result;
}

static auto create_market_data(
    Gateway &gateway, core::io::Context &context, uint16_t &stream_id, Shared &shared) {
  std::vector<std::unique_ptr<MarketData>> result;
  auto market_data = std::make_unique<MarketData>(gateway, context, ++stream_id, shared, true);
  result.emplace_back(std::move(market_data));
  return result;
}
}  // namespace

Gateway::Gateway(server::Dispatcher &dispatcher, const Config &config)
    : dispatcher_(dispatcher), master_account_(config.get_master_account()),
      security_(create_security(config)), shared_(dispatcher),
      drop_copy_(create_drop_copy(*this, context_, stream_id_, security_, shared_)),
      market_data_(create_market_data(*this, context_, stream_id_, shared_)) {
}

void Gateway::operator()(const Event<Start> &event) {
  log::info("Starting the gateway..."sv);
  for (auto &[_, drop_copy] : drop_copy_)
    if (static_cast<bool>(drop_copy))
      (*drop_copy)(event);
  for (auto &market_data : market_data_)
    (*market_data)(event);
}

void Gateway::operator()(const Event<Stop> &event) {
  log::info("Stopping the gateway..."sv);
  for (auto &market_data : market_data_)
    (*market_data)(event);
  for (auto &[_, drop_copy] : drop_copy_)
    if (static_cast<bool>(drop_copy))
      (*drop_copy)(event);
}

void Gateway::operator()(const Event<Timer> &event) {
  for (auto &[_, drop_copy] : drop_copy_)
    if (static_cast<bool>(drop_copy))
      (*drop_copy)(event);
  for (auto &market_data : market_data_)
    (*market_data)(event);
  context_.dispatch(true);
}

void Gateway::operator()(const Event<Connected> &) {
}

void Gateway::operator()(const Event<Disconnected> &event) {
  const auto &[message_info, disconnected] = event;
  log::warn(
      R"(Disconnected: source="{}", order_cancel_policy={})"sv,
      message_info.source_name,
      disconnected.order_cancel_policy);
  switch (disconnected.order_cancel_policy) {
    case OrderCancelPolicy::UNDEFINED:
      break;
    case OrderCancelPolicy::MANAGED_ORDERS:
      log::warn("*** CANCEL MANAGED ORDERS NOT IMPLEMENTED ***"sv);
      break;
    case OrderCancelPolicy::BY_ACCOUNT:
      log::warn("*** CANCEL ALL ACCOUNT ORDERS ***"sv);
      for (auto &[account, drop_copy] : drop_copy_) {
        if (dispatcher_.can_user_trade_account(account, message_info.source)) {
          log::warn(R"(- account="{}")"sv, account);
          CancelAllOrders cancel_all_orders{
              .account = account,
          };
          Event event(message_info, cancel_all_orders);
          (*drop_copy)(event, {});
        }
      }
  }
}

void Gateway::operator()(const server::Trace<StreamStatus> &event) {
  dispatcher_(event);
}

void Gateway::operator()(const server::Trace<ExternalLatency> &event) {
  dispatcher_(event);
}

void Gateway::operator()(const server::Trace<ReferenceData> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(const server::Trace<MarketStatus> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(const server::Trace<TopOfBook> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(
    const server::Trace<MarketByPriceUpdate> &event, bool is_last, bool refresh) {
  dispatcher_(
      event,
      is_last,
      refresh,
      shared_.final_bids,
      shared_.final_asks,
      []([[maybe_unused]] auto &market_by_price) {});
}

void Gateway::operator()(const server::Trace<TradeSummary> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(const server::Trace<StatisticsUpdate> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(const server::Trace<TradeUpdate> &event, bool is_last, uint8_t user_id) {
  dispatcher_(event, is_last, user_id);
}

void Gateway::operator()(const server::Trace<FundsUpdate> &event, bool is_last) {
  dispatcher_(event, is_last);
}

void Gateway::operator()(MarketData::SymbolsUpdate &symbols_update) {
  auto &symbols = symbols_update.symbols;
  for (auto &iter : market_data_) {
    if (std::empty(symbols))
      break;
    (*iter).update_subscriptions(symbols);
  }
  for (;;) {
    if (std::empty(symbols))
      break;
    log::info("Create market-data (public channel)"sv);
    auto market_data = std::make_unique<MarketData>(*this, context_, ++stream_id_, shared_, false);
    (*market_data).update_subscriptions(symbols);
    MessageInfo message_info;  // XXX something sensible
    Start start;
    create_event_and_dispatch(*market_data, message_info, start);
    market_data_.emplace_back(std::move(market_data));
  }
}

uint16_t Gateway::operator()(
    const Event<CreateOrder> &event, const oms::Order &order, const std::string_view &request_id) {
  assert(!std::empty(event.value.account));
  return get_drop_copy(event.value.account)(event, order, request_id);
}

uint16_t Gateway::operator()(
    const Event<ModifyOrder> &event,
    const oms::Order &order,
    const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_drop_copy(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    const Event<CancelOrder> &event,
    const oms::Order &order,
    const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  assert(!std::empty(event.value.account));
  assert(event.value.account == order.account);
  return get_drop_copy(event.value.account)(event, order, request_id, previous_request_id);
}

uint16_t Gateway::operator()(
    const Event<CancelAllOrders> &event, const std::string_view &request_id) {
  assert(!std::empty(event.value.account));
  return get_drop_copy(event.value.account)(event, request_id);
}

void Gateway::operator()(metrics::Writer &writer) {
  for (auto &[_, drop_copy] : drop_copy_)
    if (static_cast<bool>(drop_copy))
      (*drop_copy)(writer);
  for (auto &iter : market_data_)
    (*iter)(writer);
}

DropCopy &Gateway::get_drop_copy(const std::string_view &account) {
  auto iter = drop_copy_.find(account);
  if (iter != std::end(drop_copy_))
    return *(*iter).second;
  throw RuntimeErrorException(R"(Unknown account="{}")"sv, account);
}

}  // namespace okex
}  // namespace roq
