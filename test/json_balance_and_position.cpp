/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::BalanceAndPosition;

TEST_CASE("simple", "[json_balance_and_position]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"balance_and_position",)"
                 R"("uid":"33594834598109184")"
                 R"(},)"
                 R"("data":[{)"
                 R"("balData":[{)"
                 R"("cashBal":"0.0121475",)"
                 R"("ccy":"BTC",)"
                 R"("uTime":"1640088676388")"
                 R"(},{)"
                 R"("cashBal":"0.00000635",)"
                 R"("ccy":"OMG",)"
                 R"("uTime":"1624937815126")"
                 R"(},{)"
                 R"("cashBal":"0.00005916",)"
                 R"("ccy":"EOS",)"
                 R"("uTime":"1624937815179")"
                 R"(})"
                 R"(],)"
                 R"("eventType":"snapshot",)"
                 R"("pTime":"1640151123380",)"
                 R"("posData":[])"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::BALANCE_AND_POSITION);
    CHECK(obj.arg.uid == "33594834598109184"sv);
    REQUIRE(std::size(obj.data) == 1);
    auto &d0 = obj.data[0];
    auto &bal_data = d0.bal_data;
    REQUIRE(std::size(bal_data) == 3);
    auto &bal_0 = bal_data[0];
    CHECK(bal_0.cash_bal == 0.0121475_a);
    CHECK(bal_0.ccy == "BTC"sv);
    CHECK(bal_0.u_time == 1640088676388ms);
    auto &bal_1 = bal_data[1];
    CHECK(bal_1.cash_bal == 0.00000635_a);
    CHECK(bal_1.ccy == "OMG"sv);
    CHECK(bal_1.u_time == 1624937815126ms);
    auto &bal_2 = bal_data[2];
    CHECK(bal_2.cash_bal == 0.00005916_a);
    CHECK(bal_2.ccy == "EOS"sv);
    CHECK(bal_2.u_time == 1624937815179ms);
    //
    CHECK(d0.event_type == "snapshot"sv);
    CHECK(d0.p_time == 1640151123380ms);
    auto &pos_data = d0.pos_data;
    REQUIRE(std::size(pos_data) == 0);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}

TEST_CASE("simple_2", "[json_balance_and_position]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"balance_and_position",)"
                 R"("uid":"187956862690435072")"
                 R"(},)"
                 R"("data":[{)"
                 R"("balData":[{)"
                 R"("cashBal":"200.5137143201668551",)"
                 R"("ccy":"USDT",)"
                 R"("uTime":"1644330337903")"
                 R"(})"
                 R"(],)"
                 R"("eventType":"snapshot",)"
                 R"("pTime":"1644330371092",)"
                 R"("posData":[{)"
                 R"("avgPx":"43684",)"
                 R"("baseBal":"",)"
                 R"("ccy":"USDT",)"
                 R"("instId":"BTC-USDT-SWAP",)"
                 R"("instType":"SWAP",)"
                 R"("mgnMode":"cross",)"
                 R"("pos":"1",)"
                 R"("posCcy":"",)"
                 R"("posId":"325382268437037058",)"
                 R"("posSide":"net",)"
                 R"("quoteBal":"",)"
                 R"("tradeId":"186646171",)"
                 R"("uTime":"1644330337903")"
                 R"(})"
                 R"(])"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::BALANCE_AND_POSITION);
    CHECK(obj.arg.uid == "187956862690435072"sv);
    REQUIRE(std::size(obj.data) == 1);
    auto &d0 = obj.data[0];
    REQUIRE(std::size(d0.bal_data) == 1);
    auto &bal_0 = d0.bal_data[0];
    CHECK(bal_0.cash_bal == 200.5137143201668551_a);
    CHECK(bal_0.ccy == "USDT"sv);
    CHECK(bal_0.u_time == 1644330337903ms);
    CHECK(d0.event_type == "snapshot"sv);
    CHECK(d0.p_time == 1644330371092ms);
    auto &pos_data = d0.pos_data;
    REQUIRE(std::size(pos_data) == 1);
    auto &pos_0 = pos_data[0];
    CHECK(pos_0.avg_px == 43684.0_a);
    CHECK(std::isnan(pos_0.base_bal) == true);
    CHECK(pos_0.ccy == "USDT"sv);
    CHECK(pos_0.inst_id == "BTC-USDT-SWAP"sv);
    CHECK(pos_0.inst_type == protocol::json::InstrumentType::SWAP);
    CHECK(pos_0.mgn_mode == protocol::json::MarginMode::CROSS);
    CHECK(pos_0.pos == 1.0_a);
    CHECK(std::empty(pos_0.pos_ccy));
    CHECK(pos_0.pos_id == "325382268437037058"sv);
    CHECK(pos_0.pos_side == protocol::json::PositionSide::NET);
    CHECK(std::isnan(pos_0.quote_bal) == true);
    CHECK(pos_0.trade_id == "186646171"sv);
    CHECK(pos_0.u_time == 1644330337903ms);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
