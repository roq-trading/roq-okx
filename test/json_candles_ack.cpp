/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/okx/protocol/json/candles_ack.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::CandlesAck;

// note! truncated
TEST_CASE("json_candles", "[json_candles_ack]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("msg":"",)"
                 R"("data":[)"
                 R"(["1757076060000","112647.6","112647.6","112501.5","112501.5","0.72920729","82057.137546757","82057.137546757","0"],)"
                 R"(["1757076000000","112691.9","112762","112628","112628","2.98698034","336525.112664131","336525.112664131","1"],)"
                 R"(["1757058180000","112770","112800","112770","112796","0.22385039","25248.95053723","25248.95053723","1"],)"
                 R"(["1757058120000","112769.5","112769.5","112769.5","112769.5","0","0","0","1"])"
                 R"(])"
                 R"(})";
  auto helper = [&](value_type &obj) {
    CHECK(obj.code == 0);
    CHECK(std::empty(obj.msg));
    REQUIRE(std::size(obj.data) == 4);
    auto &d = obj.data;
    CHECK(d[0].timestamp == 1757076060000ms);
    CHECK(d[3].timestamp == 1757058120000ms);
  };
  core::json::BufferStack buffers{8192, 1};
  value_type obj{message, buffers};
  helper(obj);
}
