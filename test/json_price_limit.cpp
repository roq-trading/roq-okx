/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::PriceLimit;
/*
TEST_CASE("simple", "[json_price_limit]") {
  auto message = R"({)"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::ESTIMATED_PRICE);
    CHECK(obj.arg.inst_id == "BTC-USDT"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
*/
