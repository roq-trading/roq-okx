/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/tickers.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_tickers, simple) {
  auto message = R"([)"
                 R"({)"
                 R"("instType":"SPOT",)"
                 R"("instId":"HC-USDT",)"
                 R"("last":"0.5041",)"
                 R"("lastSz":"28.923426",)"
                 R"("askPx":"0.5044",)"
                 R"("askSz":"5.661856",)"
                 R"("bidPx":"0.504",)"
                 R"("bidSz":"111.121317",)"
                 R"("open24h":"0.5112",)"
                 R"("high24h":"0.5242",)"
                 R"("low24h":"0.5",)"
                 R"("sodUtc0":"0.5072",)"
                 R"("sodUtc8":"0.5137",)"
                 R"("volCcy24h":"268200.641648",)"
                 R"("vol24h":"526157.724396",)"
                 R"("ts":"1639319049926")"
                 R"(})"
                 R"(])";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Tickers>(message, buffer_);
  ASSERT_EQ(std::size(obj.data), 1);
  auto &data = obj.data[0];
  EXPECT_EQ(data.inst_type, json::InstrumentType::SPOT);
  EXPECT_EQ(data.inst_id, "HC-USDT"sv);
  EXPECT_DOUBLE_EQ(data.last, 0.5041);
  EXPECT_DOUBLE_EQ(data.last_sz, 28.923426);
  EXPECT_DOUBLE_EQ(data.ask_px, 0.5044);
  EXPECT_DOUBLE_EQ(data.ask_sz, 5.661856);
  EXPECT_DOUBLE_EQ(data.bid_px, 0.504);
  EXPECT_DOUBLE_EQ(data.bid_sz, 111.121317);
  EXPECT_DOUBLE_EQ(data.open24h, 0.5112);
  EXPECT_DOUBLE_EQ(data.high24h, 0.5242);
  EXPECT_DOUBLE_EQ(data.low24h, 0.5);
  EXPECT_DOUBLE_EQ(data.sod_utc0, 0.5072);
  EXPECT_DOUBLE_EQ(data.sod_utc8, 0.5137);
  EXPECT_DOUBLE_EQ(data.vol_ccy24h, 268200.641648);
  EXPECT_DOUBLE_EQ(data.vol24h, 526157.724396);
  EXPECT_EQ(data.ts, 1639319049926ms);
}
