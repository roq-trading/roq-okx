/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::ChannelConnCount;

TEST_CASE("simple", "[json_channel_conn_count]") {
  auto message = R"({)"
                 R"("event":"channel-conn-count",)"
                 R"("channel":"account",)"
                 R"("connCount":"1",)"
                 R"("connId":"4adc328c")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == json::EventType::CHANNEL_CONN_COUNT);
    CHECK(obj.channel == json::Channel::ACCOUNT);
    CHECK(obj.conn_count == 1);
    CHECK(obj.conn_id == "4adc328c"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
