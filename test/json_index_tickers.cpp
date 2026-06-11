/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::IndexTickers;

TEST_CASE("simple", "[json_index_tickers]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"index-tickers",)"
                 R"("instId":"LTC-USD")"
                 R"(},)"
                 R"("data":[{)"
                 R"("instId":"LTC-USD",)"
                 R"("idxPx":"83.79",)"
                 R"("open24h":"81.74",)"
                 R"("high24h":"84.15",)"
                 R"("low24h":"79.93",)"
                 R"("sodUtc0":"81.44",)"
                 R"("sodUtc8":"81.65",)"
                 R"("ts":"1765201257112")"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::INDEX_TICKERS);
    CHECK(obj.arg.inst_id == "LTC-USD"sv);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
