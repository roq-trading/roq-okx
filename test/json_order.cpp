/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::Order;

TEST_CASE("success", "[json_order]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"abcABC123",)"
                 R"("ordId":"393513242072608785",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"",)"
                 R"("tag":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"1000001",)"
                 R"("msg":"",)"
                 R"("op":"order")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.code == 0);
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.cl_ord_id == "abcABC123"sv);
    CHECK(data_0.ord_id == "393513242072608785"sv);
    CHECK(data_0.s_code == 0);
    CHECK(std::empty(data_0.s_msg));
    CHECK(std::empty(data_0.tag));
    CHECK(obj.id == "1000001"sv);
    CHECK(std::empty(obj.msg));
    CHECK(obj.op == protocol::json::Operation::ORDER);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}

TEST_CASE("failure", "[json_order]") {
  auto message = R"({)"
                 R"("code":"1",)"
                 R"("data":[{)"
                 R"("clOrdId":"abcABC125",)"
                 R"("ordId":"",)"
                 R"("sCode":"51016",)"
                 R"("sMsg":"Duplicated client order ID",)"
                 R"("tag":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000001",)"
                 R"("msg":"",)"
                 R"("op":"order")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.code == 1);
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.cl_ord_id == "abcABC125"sv);
    CHECK(std::empty(data_0.ord_id));
    CHECK(data_0.s_code == 51016);
    CHECK(data_0.s_msg == "Duplicated client order ID"sv);
    CHECK(std::empty(data_0.tag));
    CHECK(obj.id == "2000001"sv);
    CHECK(std::empty(obj.msg));
    CHECK(obj.op == protocol::json::Operation::ORDER);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
