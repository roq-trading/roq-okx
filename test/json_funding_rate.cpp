/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::FundingRate;

TEST_CASE("simple", "[json_funding_rate]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"funding-rate",)"
                 R"("instId":"LTC-USDT-SWAP")"
                 R"(},)"
                 R"("data":[{)"
                 R"("formulaType":"withRate",)"
                 R"("fundingRate":"0.0001000000000000",)"
                 R"("fundingTime":"1765209600000",)"
                 R"("impactValue":"10000.0000000000000000",)"
                 R"("instId":"LTC-USDT-SWAP",)"
                 R"("instType":"SWAP",)"
                 R"("interestRate":"0.0001000000000000",)"
                 R"("maxFundingRate":"0.015",)"
                 R"("method":"current_period",)"
                 R"("minFundingRate":"-0.015",)"
                 R"("nextFundingRate":"",)"
                 R"("nextFundingTime":"1765238400000",)"
                 R"("premium":"-0.0004021053670636",)"
                 R"("settFundingRate":"0.0000697725497563",)"
                 R"("settState":"settled",)"
                 R"("ts":"1765201461438")"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::FUNDING_RATE);
    CHECK(obj.arg.inst_id == "LTC-USDT-SWAP"sv);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
