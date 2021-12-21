/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/orders.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

// note! only data section

TEST(json_orders, new_order) {
  auto message = R"([{)"
                 R"("accFillSz":"0",)"
                 R"("amendResult":"",)"
                 R"("avgPx":"0",)"
                 R"("cTime":"1640093742566",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"abcABC124",)"
                 R"("code":"0",)"
                 R"("execType":"",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillFee":"0",)"
                 R"("fillFeeCcy":"",)"
                 R"("fillNotionalUsd":"",)"
                 R"("fillPx":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("instType":"FUTURES",)"
                 R"("lever":"10",)"
                 R"("msg":"",)"
                 R"("notionalUsd":"100.0",)"
                 R"("ordId":"393516910134063109",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"long",)"
                 R"("px":"40225.4",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"BTC",)"
                 R"("reduceOnly":"false",)"
                 R"("reqId":"",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"live",)"  // note!
                 R"("sz":"1",)"
                 R"("tag":"",)"
                 R"("tdMode":"isolated",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1640093742566")"
                 R"(})"
                 R"(])";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Orders>(message, buffer_);
  ASSERT_EQ(std::size(obj.data), 1);
  auto &data = obj.data[0];
}

TEST(json_orders, cancel_order) {
  auto message = R"([{)"
                 R"("accFillSz":"0",)"
                 R"("amendResult":"",)"
                 R"("avgPx":"0",)"
                 R"("cTime":"1640096276280",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"abcABC125",)"
                 R"("code":"0",)"
                 R"("execType":"",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillFee":"0",)"
                 R"("fillFeeCcy":"",)"
                 R"("fillNotionalUsd":"",)"
                 R"("fillPx":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("instType":"FUTURES",)"
                 R"("lever":"10",)"
                 R"("msg":"",)"
                 R"("notionalUsd":"100.0",)"
                 R"("ordId":"393527537300828177",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"long",)"
                 R"("px":"40046.2",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"BTC",)"
                 R"("reduceOnly":"false",)"
                 R"("reqId":"",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"canceled",)"  // note!
                 R"("sz":"1",)"
                 R"("tag":"",)"
                 R"("tdMode":"isolated",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1640096470331")"
                 R"(})"
                 R"(])";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Orders>(message, buffer_);
  ASSERT_EQ(std::size(obj.data), 1);
  auto &data = obj.data[0];
}
