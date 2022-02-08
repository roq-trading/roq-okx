/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roq/server.h"

#include "roq/core/io/context.h"

#include "roq/okx/config.h"
#include "roq/okx/drop_copy.h"
#include "roq/okx/market_data.h"
#include "roq/okx/order_entry.h"
#include "roq/okx/security.h"
#include "roq/okx/shared.h"

namespace roq {
namespace okx {

class Gateway final : public server::Handler,
                      public DropCopy::Handler,
                      public OrderEntry::Handler,
                      public MarketData::Handler {
 public:
  Gateway(server::Dispatcher &, const Config &);

 protected:
  void operator()(const Event<Start> &) override;
  void operator()(const Event<Stop> &) override;
  void operator()(const Event<Timer> &) override;
  void operator()(const Event<Connected> &) override;
  void operator()(const Event<Disconnected> &) override;

  uint16_t operator()(
      const Event<CreateOrder> &, const oms::Order &, const std::string_view &request_id) override;
  uint16_t operator()(
      const Event<ModifyOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;
  uint16_t operator()(
      const Event<CancelOrder> &,
      const oms::Order &,
      const std::string_view &request_id,
      const std::string_view &previous_request_id) override;

  uint16_t operator()(const Event<CancelAllOrders> &, const std::string_view &request_id) override;

  void operator()(metrics::Writer &) override;

  // many

  void operator()(server::Trace<StreamStatus> const &) override;
  void operator()(server::Trace<ExternalLatency> const &) override;
  void operator()(server::Trace<ReferenceData> const &, bool is_last) override;
  void operator()(server::Trace<MarketStatus> const &, bool is_last) override;
  void operator()(server::Trace<TopOfBook> const &, bool is_last) override;
  void operator()(server::Trace<MarketByPriceUpdate> const &, bool is_last, bool refresh) override;
  void operator()(server::Trace<TradeSummary> const &, bool is_last) override;
  void operator()(server::Trace<StatisticsUpdate> const &, bool is_last) override;
  void operator()(const server::Trace<TradeUpdate> &, bool is_last, uint8_t user_id) override;
  void operator()(server::Trace<FundsUpdate> const &, bool is_last) override;
  void operator()(server::Trace<PositionUpdate> const &, bool is_last) override;

  void operator()(MarketData::SymbolsUpdate &) override;

  void ensure_symbol_slices(size_t size);

  // utilities

  OrderEntry &get_order_entry(const std::string_view &account);

 private:
  server::Dispatcher &dispatcher_;
  // config
  const std::string master_account_;
  // security
  absl::flat_hash_map<std::string, std::unique_ptr<Security>> security_;
  // io
  core::io::Context context_;
  // shared
  Shared shared_;
  // seed
  uint16_t stream_id_ = {};
  // streams
  absl::flat_hash_map<std::string, std::unique_ptr<DropCopy>> drop_copy_;
  absl::flat_hash_map<std::string, std::unique_ptr<OrderEntry>> order_entry_;
  std::vector<std::unique_ptr<MarketData>> market_data_;
};

}  // namespace okx
}  // namespace roq
