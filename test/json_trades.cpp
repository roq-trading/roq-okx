/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Trades;

TEST_CASE("json_trades_parser", "[json_trades]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"trades",)"
                 R"("instId":"BTC-USDT-SWAP"},)"
                 R"("data":[{)"
                 R"("instId":"BTC-USDT-SWAP",)"
                 R"("tradeId":"2005639579",)"
                 R"("px":"91467.1",)"
                 R"("sz":"0.03",)"
                 R"("side":"sell",)"
                 R"("ts":"1765201257145",)"
                 R"("count":"1",)"
                 R"("source":"0",)"
                 R"("seqId":279183506666)"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::TRADES);
    CHECK(obj.arg.inst_id == "BTC-USDT-SWAP"sv);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
