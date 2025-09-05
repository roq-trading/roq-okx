/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/core/json/buffer_stack.hpp"

#include "roq/okx/json/candles.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_candles", "[json_candles]") {
  // note! truncated
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
  core::json::BufferStack buffer{8192, 1};
  json::Candles candles{message, buffer};
  CHECK(candles.code == 0);
  CHECK(std::empty(candles.msg));
  REQUIRE(std::size(candles.data) == 4);
  auto &d = candles.data;
  CHECK(d[0].timestamp == 1757076060000ms);
  CHECK(d[3].timestamp == 1757058120000ms);
}
