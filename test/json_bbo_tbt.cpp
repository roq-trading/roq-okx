/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::BboTbt;

TEST_CASE("simple", "[json_bbo_tbt]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"bbo-tbt",)"
                 R"("instId":"LTC-USDC")"
                 R"(},)"
                 R"("data":[{)"
                 R"("asks":[)"
                 R"(["83.81","11.93175","0","1"])"
                 R"(],)"
                 R"("bids":[)"
                 R"(["83.8","11.93317","0","1"])"
                 R"(],)"
                 R"("ts":"1765201257149",)"
                 R"("seqId":378617009)"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::BBO_TBT);
    CHECK(obj.arg.inst_id == "LTC-USDC"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
