/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/okx/protocol/json/orders_pending_ack.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::OrdersPendingAck;

TEST_CASE("empty", "[json_orders_pending_ack]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[],)"
                 R"("msg":"")"
                 R"(})";
  auto helper = [&](value_type &obj) {
    CHECK(obj.code == 0);
    REQUIRE(std::size(obj.data) == 0);
    CHECK(std::empty(obj.msg));
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}

TEST_CASE("simple", "[json_orders_pending_ack]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("accFillSz":"0",)"
                 R"("avgPx":"",)"
                 R"("cTime":"1640182694746",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"abcABC125",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillPx":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("instType":"FUTURES",)"
                 R"("lever":"10",)"
                 R"("ordId":"393890002618445825",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"long",)"
                 R"("px":"39919.4",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"BTC",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"live",)"
                 R"("sz":"1",)"
                 R"("tag":"",)"
                 R"("tdMode":"isolated",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1640182694746")"
                 R"(})"
                 R"(],)"
                 R"("msg":"")"
                 R"(})";
  auto helper = [&](value_type &obj) {
    CHECK(obj.code == 0);
    REQUIRE(std::size(obj.data) == 1);
    // XXX HANS
    CHECK(std::empty(obj.msg));
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
