/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Tickers;

TEST_CASE("simple", "[json_tickers]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"tickers",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("data":[{)"
                 R"("instType":"SPOT",)"
                 R"("instId":"BTC-USD",)"
                 R"("last":"91517.2",)"
                 R"("lastSz":"0.00269444",)"
                 R"("askPx":"91527.3",)"
                 R"("askSz":"1.54613591",)"
                 R"("bidPx":"91527.2",)"
                 R"("bidSz":"0.0135",)"
                 R"("open24h":"88979.1",)"
                 R"("high24h":"92327.7",)"
                 R"("low24h":"87758.1",)"
                 R"("sodUtc0":"90420.2",)"
                 R"("sodUtc8":"89574.2",)"
                 R"("volCcy24h":"30744587.593360288",)"
                 R"("vol24h":"338.86622649",)"
                 R"("ts":"1765201256914")"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::TICKERS);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
