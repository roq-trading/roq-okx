/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::Notice;

TEST_CASE("simple", "[json_notice]") {
  auto message = R"({)"
                 R"("event":"notice",)"
                 R"("msg":"The connection will soon be closed for a service upgrade. Please reconnect.",)"
                 R"("code":"64008",)"
                 R"("connId":"8517149b")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::NOTICE);
    CHECK(obj.msg == "The connection will soon be closed for a service upgrade. Please reconnect."sv);
    CHECK(obj.code == 64008);
    CHECK(obj.conn_id == "8517149b"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
