/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::AmendOrder;

TEST_CASE("success", "[json_amend_order]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"CMAAF2IDAAAQAAFQDIHPD4Y3",)"
                 R"("ordId":"393936310213439488",)"
                 R"("reqId":"",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000002",)"
                 R"("msg":"",)"
                 R"("op":"amend-order")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.code == 0);
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.cl_ord_id == "CMAAF2IDAAAQAAFQDIHPD4Y3"sv);
    CHECK(data_0.ord_id == "393936310213439488"sv);
    CHECK(std::empty(data_0.req_id));
    CHECK(data_0.s_code == 0);
    CHECK(std::empty(data_0.s_msg));
    CHECK(std::empty(data_0.tag));
    CHECK(obj.id == "2000002"sv);
    CHECK(std::empty(obj.msg));
    CHECK(obj.op == protocol::json::Operation::AMEND_ORDER);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
