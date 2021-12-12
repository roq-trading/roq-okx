/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/parser.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

TEST(json_spot_ticker, simple) {
  auto message = R"({)"
                 R"("table":"spot/ticker",)"
                 R"("data":[{)"
                 R"("last":"4026.43",)"
                 R"("open_24h":"3995.96",)"
                 R"("best_bid":"4026.42",)"
                 R"("high_24h":"4096.32",)"
                 R"("low_24h":"3834.19",)"
                 R"("open_utc0":"3897.92",)"
                 R"("open_utc8":"4058.26",)"
                 R"("base_volume_24h":"91000.057673",)"
                 R"("quote_volume_24h":"363305028.75633",)"
                 R"("best_ask":"4026.43",)"
                 R"("instrument_id":"ETH-USDT",)"
                 R"("timestamp":"2021-12-11T18:22:23.074Z",)"
                 R"("best_bid_size":"1.43199",)"
                 R"("best_ask_size":"0.62",)"
                 R"("last_qty":"0.001839")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct Handler : public json::Parser::Handler {
    void operator()(server::Trace<json::Error> const &) override {}
    void operator()(server::Trace<json::Subscribe> const &) override {}
    void operator()(server::Trace<json::Unsubscribe> const &) override {}
    void operator()(server::Trace<json::SpotTicker> const &event) override {
      auto &[trace_info, spot_ticker] = event;
      ASSERT_EQ(found, false);
      found = true;
      EXPECT_DOUBLE_EQ(spot_ticker.last, 4026.43);
      EXPECT_DOUBLE_EQ(spot_ticker.open_24h, 3995.96);
      EXPECT_DOUBLE_EQ(spot_ticker.best_bid, 4026.42);
      EXPECT_DOUBLE_EQ(spot_ticker.high_24h, 4096.32);
      EXPECT_DOUBLE_EQ(spot_ticker.low_24h, 3834.19);
      EXPECT_DOUBLE_EQ(spot_ticker.open_utc0, 3897.92);
      EXPECT_DOUBLE_EQ(spot_ticker.open_utc8, 4058.26);
      EXPECT_DOUBLE_EQ(spot_ticker.base_volume_24h, 91000.057673);
      EXPECT_DOUBLE_EQ(spot_ticker.quote_volume_24h, 363305028.75633);
      EXPECT_DOUBLE_EQ(spot_ticker.best_ask, 4026.43);
      EXPECT_EQ(spot_ticker.instrument_id, "ETH-USDT"sv);
      EXPECT_EQ(spot_ticker.timestamp, 1639246943074ms);
      EXPECT_DOUBLE_EQ(spot_ticker.best_bid_size, 1.43199);
      EXPECT_DOUBLE_EQ(spot_ticker.best_ask_size, 0.62);
      EXPECT_DOUBLE_EQ(spot_ticker.last_qty, 0.001839);
    }
    void operator()(server::Trace<json::SpotTrade> const &) override {}
    void operator()(server::Trace<json::SpotDepthL2Tbt> const &, json::Action) override {}
    bool found = false;
  } handler;
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto res = json::Parser::dispatch(handler, message, buffer_, {});
  EXPECT_EQ(res, true);
  EXPECT_EQ(handler.found, true);
}
