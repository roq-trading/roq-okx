/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/instruments.h"

using namespace roq;
using namespace roq::okex;

using namespace std::literals;
using namespace std::chrono_literals;

// note! reduced
TEST(json_instruments_swaps, simple) {
  auto message = R"([{)"
                 R"("alias":"",)"
                 R"("baseCcy":"",)"
                 R"("category":"1",)"
                 R"("ctMult":"1",)"
                 R"("ctType":"inverse",)"
                 R"("ctVal":"100",)"
                 R"("ctValCcy":"USD",)"
                 R"("expTime":"",)"
                 R"("instId":"BTC-USD-SWAP",)"
                 R"("instType":"SWAP",)"
                 R"("lever":"125",)"
                 R"("listTime":"1636097920000",)"
                 R"("lotSz":"1",)"
                 R"("minSz":"1",)"
                 R"("optType":"",)"
                 R"("quoteCcy":"",)"
                 R"("settleCcy":"BTC",)"
                 R"("state":"live",)"
                 R"("stk":"",)"
                 R"("tickSz":"0.1",)"
                 R"("uly":"BTC-USD")"
                 R"(},{)"
                 R"("alias":"",)"
                 R"("baseCcy":"",)"
                 R"("category":"1",)"
                 R"("ctMult":"1",)"
                 R"("ctType":"inverse",)"
                 R"("ctVal":"10",)"
                 R"("ctValCcy":"USD",)"
                 R"("expTime":"",)"
                 R"("instId":"ETH-USD-SWAP",)"
                 R"("instType":"SWAP",)"
                 R"("lever":"125",)"
                 R"("listTime":"1611916828000",)"
                 R"("lotSz":"1",)"
                 R"("minSz":"1",)"
                 R"("optType":"",)"
                 R"("quoteCcy":"",)"
                 R"("settleCcy":"ETH",)"
                 R"("state":"live",)"
                 R"("stk":"",)"
                 R"("tickSz":"0.01",)"
                 R"("uly":"ETH-USD")"
                 R"(})"
                 R"(])";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Instruments>(message, buffer_);
}
