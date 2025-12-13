/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Error;

TEST_CASE("login", "[json_error]") {
  auto message = R"({)"
                 R"("event":"error",)"
                 R"("msg":"API key doesn't exist",)"
                 R"("code":"60032",)"
                 R"("connId":"f9ab6f3c")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == json::EventType::ERROR);
    CHECK(obj.msg == "API key doesn't exist"sv);
    CHECK(obj.code == 60032);
    CHECK(obj.conn_id == "f9ab6f3c"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("subscription", "[json_error]") {
  auto message =
      R"({)"
      R"("event":"error",)"
      R"("msg":"Wrong URL or channel:index-tickers, instId:NEAR-USDT-SWAP doesn't exist. Please use the correct URL, channel and parameters referring to API document.",)"
      R"("code":"60018",)"
      R"("connId":"94d38802")"
      R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == json::EventType::ERROR);
    CHECK(
        obj.msg ==
        R"(Wrong URL or channel:index-tickers, instId:NEAR-USDT-SWAP doesn't exist. Please use the correct URL, channel and parameters referring to API document.)");
    CHECK(obj.code == 60018);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
