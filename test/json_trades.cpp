/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/trades.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_trades, simple) {
  auto message = R"([)"
                 R"({)"
                 R"("instId":"HC-USDT",)"
                 R"("tradeId":"7100137",)"
                 R"("px":"0.5043",)"
                 R"("sz":"1.452059",)"
                 R"("side":"sell",)"
                 R"("ts":"1639320265519")"
                 R"(})"
                 R"(])";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Trades>(message, buffer_);
  ASSERT_EQ(std::size(obj.data), 1);
  auto &data = obj.data[0];
  EXPECT_EQ(data.inst_id, "HC-USDT"sv);
  EXPECT_EQ(data.trade_id, "7100137"sv);
  EXPECT_DOUBLE_EQ(data.px, 0.5043);
  EXPECT_DOUBLE_EQ(data.sz, 1.452059);
  EXPECT_EQ(data.side, json::Side::SELL);
  EXPECT_EQ(data.ts, 1639320265519ms);
}
