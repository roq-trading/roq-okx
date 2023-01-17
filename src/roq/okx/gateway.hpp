/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/server.hpp"

#include "roq/io/context.hpp"

#include "roq/okx/config.hpp"
#include "roq/okx/market_data.hpp"
#include "roq/okx/order_entry.hpp"
#include "roq/okx/request.hpp"
#include "roq/okx/rest.hpp"
#include "roq/okx/security.hpp"
#include "roq/okx/shared.hpp"

namespace roq {
namespace okx {

struct Gateway final : public server::Handler,
                       public Rest::Handler,
                       public OrderEntry::Handler,
                       public MarketData::Handler {
  Gateway(server::Dispatcher &, Config const &, io::Context &);

 protected:
  void operator()(Event<Start> const &) override;
  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;
  void operator()(Event<Connected> const &) override;
  void operator()(Event<Disconnected> const &) override;

  uint16_t operator()(Event<CreateOrder> const &, oms::Order const &, std::string_view const &request_id) override;
  uint16_t operator()(
      Event<ModifyOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;
  uint16_t operator()(
      Event<CancelOrder> const &,
      oms::Order const &,
      std::string_view const &request_id,
      std::string_view const &previous_request_id) override;

  uint16_t operator()(Event<CancelAllOrders> const &, std::string_view const &request_id) override;

  void operator()(metrics::Writer &) override;

  // many

  void operator()(Trace<StreamStatus> const &) override;
  void operator()(Trace<ExternalLatency> const &) override;
  void operator()(Trace<ReferenceData> const &, bool is_last) override;
  void operator()(Trace<MarketStatus> const &, bool is_last) override;
  void operator()(Trace<TopOfBook> const &, bool is_last) override;
  void operator()(Trace<MarketByPriceUpdate> const &, bool is_last, bool refresh) override;
  void operator()(Trace<TradeSummary> const &, bool is_last) override;
  void operator()(Trace<StatisticsUpdate> const &, bool is_last) override;
  void operator()(Trace<TradeUpdate> const &, bool is_last, uint8_t user_id) override;
  void operator()(Trace<FundsUpdate> const &, bool is_last) override;
  void operator()(Trace<PositionUpdate> const &, bool is_last) override;

  void operator()(MarketData::SymbolsUpdate &) override;

  void ensure_symbol_slices(size_t size);

  // utilities

  OrderEntry &get_order_entry(std::string_view const &account);

 private:
  server::Dispatcher &dispatcher_;
  // config
  const std::string master_account_;
  // security
  absl::flat_hash_map<Account, std::unique_ptr<Security>> security_;
  // io
  io::Context &context_;
  // shared
  Shared shared_;
  absl::flat_hash_map<Account, Request> request_;
  // seed
  uint16_t stream_id_ = {};
  // streams
  absl::flat_hash_map<Account, std::unique_ptr<Rest>> rest_;
  absl::flat_hash_map<Account, std::unique_ptr<OrderEntry>> order_entry_;
  std::vector<std::unique_ptr<MarketData>> market_data_;
};

}  // namespace okx
}  // namespace roq
