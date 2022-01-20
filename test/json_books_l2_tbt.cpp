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

TEST(json_books_l2_tbt, parser) {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"books-l2-tbt",)"
                 R"("instId":"BTC-USD-220325"},)"
                 R"("action":"snapshot",)"
                 R"("data":[{)"
                 R"("asks":[)"
                 R"(["50441.1","24","0","1"],)"
                 R"(["50449.1","152","0","3"])"
                 R"(],)"
                 R"("bids":[)"
                 R"(["50441","199","0","3"],)"
                 R"(["50438.2","49","0","1"])"
                 R"(],)"
                 R"("ts":"1640158295741",)"
                 R"("checksum":-386306878)"
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
        server::Trace<json::BooksL2Tbt> const &event,
        const std::string_view &inst_id,
        json::Action action) override {
      ++count_;
      auto &[trace_info, books_l2_tbt] = event;
      EXPECT_EQ(inst_id, "BTC-USD-220325"sv);
      EXPECT_EQ(action, json::Action::SNAPSHOT);
      auto &asks = books_l2_tbt.asks;
      ASSERT_EQ(std::size(asks), 2);
      // a0
      auto &a0 = asks[0];
      EXPECT_DOUBLE_EQ(a0.price, 50441.1);
      EXPECT_DOUBLE_EQ(a0.size, 24.0);
      EXPECT_EQ(a0.liquidated_orders, 0);
      EXPECT_EQ(a0.orders, 1);
      // a1
      auto &bids = books_l2_tbt.bids;
      ASSERT_EQ(std::size(bids), 2);
      // b0
      auto &b0 = bids[0];
      EXPECT_DOUBLE_EQ(b0.price, 50441);
      EXPECT_DOUBLE_EQ(b0.size, 199.0);
      EXPECT_EQ(b0.liquidated_orders, 0);
      EXPECT_EQ(b0.orders, 3);
    }
    void operator()(server::Trace<json::IndexTickers> const &) override { FAIL(); }
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
