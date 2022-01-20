/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/parser.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

namespace {
auto create_trace_info() {
  return server::TraceInfo{
      .source_receive_time = {},
      .origin_create_time = {},
      .origin_create_time_utc = {},
  };
}
}  // namespace

TEST(json_index_tickers, parser) {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"index-tickers",)"
                 R"("instId":"BTC-USDT")"
                 R"(},)"
                 R"("data":[{)"
                 R"("instId":"BTC-USDT",)"
                 R"("idxPx":"41756.7",)"
                 R"("open24h":"42427.3",)"
                 R"("high24h":"42560.7",)"
                 R"("low24h":"41147.5",)"
                 R"("sodUtc0":"41663.1",)"
                 R"("sodUtc8":"41984.2",)"
                 R"("ts":"1642643951284")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(server::Trace<json::Error> const &) override { FAIL(); }
    void operator()(server::Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Status> const &) override { FAIL(); }
    void operator()(server::Trace<json::Instruments> const &) override { FAIL(); }
    void operator()(server::Trace<json::EstimatedPrice> const &) override { FAIL(); }
    void operator()(server::Trace<json::PriceLimit> const &) override { FAIL(); }
    void operator()(server::Trace<json::MarkPrice> const &) override { FAIL(); }
    void operator()(server::Trace<json::Tickers> const &) override { FAIL(); }
    void operator()(server::Trace<json::Trades> const &) override { FAIL(); }
    void operator()(
        server::Trace<json::BooksL2Tbt> const &,
        [[maybe_unused]] const std::string_view &inst_id,
        json::Action) override {
      FAIL();
    }
    void operator()(server::Trace<json::IndexTickers> const &event) override {
      ++count_;
      auto &[trace_info, trades] = event;
      auto &data = trades.data;
      ASSERT_EQ(std::size(data), 1);
      auto &d0 = data[0];
      EXPECT_EQ(d0.inst_id, "BTC-USDT"sv);
      EXPECT_DOUBLE_EQ(d0.idx_px, 41756.7);
      EXPECT_DOUBLE_EQ(d0.open24h, 42427.3);
      EXPECT_DOUBLE_EQ(d0.high24h, 42560.7);
      EXPECT_DOUBLE_EQ(d0.low24h, 41147.5);
      EXPECT_DOUBLE_EQ(d0.sod_utc0, 41663.1);
      EXPECT_DOUBLE_EQ(d0.sod_utc8, 41984.2);
      EXPECT_EQ(d0.ts, 1642643951284ms);
    }
    void operator()(server::Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(server::Trace<json::Login> const &) override { FAIL(); }
    void operator()(server::Trace<json::Account> const &) override { FAIL(); }
    void operator()(server::Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(server::Trace<json::Positions> const &) override { FAIL(); }
    void operator()(server::Trace<json::Orders> const &) override { FAIL(); }
    void operator()(server::Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::CancelOrderAck> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto trace_info = create_trace_info();
  auto res = json::Parser::dispatch(handler, message, buffer_, trace_info);
  EXPECT_TRUE(res);
  EXPECT_EQ(handler.get_count(), 1);
}
