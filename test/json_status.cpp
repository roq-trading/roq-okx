/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Status;

TEST_CASE("simple", "[json_status]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"status"},)"
                 R"("data":[{)"
                 R"("begin":"1764666600000",)"
                 R"("end":"1764667800000",)"
                 R"("env":"1",)"
                 R"("href":"",)"
                 R"("maintType":"1",)"
                 R"("preOpenBegin":"",)"
                 R"("scheDesc":"",)"
                 R"("serviceType":"11",)"
                 R"("state":"completed",)"
                 R"("system":"unified",)"
                 R"("title":"Copy trading system scheduled maintenance",)"
                 R"("ts":"1764667948527")"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::STATUS);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
