/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/parser.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_spot_trade, simple) {
  auto message = R"({)"
                 R"("table":"spot/trade",)"
                 R"("data":[{)"
                 R"("side":"sell",)"
                 R"("trade_id":"157452985",)"
                 R"("price":"4042.99",)"
                 R"("size":"0.1",)"
                 R"("instrument_id":"ETH-USDT",)"
                 R"("timestamp":"2021-12-12T03:57:37.704Z")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct Handler : public json::Parser::Handler {
    void operator()(server::Trace<json::Error> const &) override {}
    void operator()(server::Trace<json::Subscribe> const &) override {}
    void operator()(server::Trace<json::Unsubscribe> const &) override {}
    void operator()(server::Trace<json::SpotTicker> const &) override {}
    void operator()(server::Trace<json::SpotTrade> const &event) override {
      auto &[trace_info, spot_trade] = event;
      ASSERT_EQ(found, false);
      found = true;
      EXPECT_EQ(spot_trade.side, json::Side::SELL);
      EXPECT_EQ(spot_trade.trade_id, "157452985"sv);
      EXPECT_DOUBLE_EQ(spot_trade.price, 4042.99);
      EXPECT_DOUBLE_EQ(spot_trade.size, 0.1);
      EXPECT_EQ(spot_trade.instrument_id, "ETH-USDT"sv);
      EXPECT_EQ(spot_trade.timestamp, 1639281457704ms);
    }
    void operator()(server::Trace<json::SpotDepthL2Tbt> const &, json::Action) override {}
    bool found = false;
  } handler;
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto res = json::Parser::dispatch(handler, message, buffer_, {});
  EXPECT_EQ(res, true);
  EXPECT_EQ(handler.found, true);
}
