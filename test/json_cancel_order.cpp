/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::CancelOrder;

TEST_CASE("success", "[json_cancel_order]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"3MAAF2IDAAAQAAGSKMZCT5A3",)"
                 R"("ordId":"393940260828377089",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000003",)"
                 R"("msg":"",)"
                 R"("op":"cancel-order")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.code == 0);
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.cl_ord_id == "3MAAF2IDAAAQAAGSKMZCT5A3"sv);
    CHECK(data_0.ord_id == "393940260828377089"sv);
    CHECK(data_0.s_code == 0);
    CHECK(std::empty(data_0.s_msg));
    CHECK(std::empty(data_0.tag));
    CHECK(obj.id == "2000003"sv);
    CHECK(std::empty(obj.msg));
    CHECK(obj.op == protocol::json::Operation::CANCEL_ORDER);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}

// {"event":"error","msg":"channel:estimated-price,instType:FUTURES doesn't exist","code":"60018"}"
