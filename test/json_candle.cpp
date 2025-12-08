/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Candle;

TEST_CASE("simple", "[json_candles]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"candle1m",)"
                 R"("instId":"BTC-USDT")"
                 R"(},)"
                 R"("data":[)"
                 R"(["1757076060000","112600.1","112631.9","112477.1","112477.1","3.10944629","349947.335430728","349947.335430728","0"])"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::CANDLE1M);
    CHECK(obj.arg.inst_id == "BTC-USDT"sv);
    REQUIRE(std::size(obj.data) == 1);
    auto &d = obj.data;
    CHECK(d[0].timestamp == 1757076060000ms);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
