/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::Account;

TEST_CASE("simple", "[json_account]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"account",)"
                 R"("uid":"33594834598109184")"
                 R"(},)"
                 R"("data":[{)"
                 R"("adjEq":"",)"
                 R"("details":[{)"
                 R"("availBal":"",)"
                 R"("availEq":"0.0121475",)"
                 R"("cashBal":"0.0121475",)"
                 R"("ccy":"BTC",)"
                 R"("coinUsdPrice":"49079",)"
                 R"("crossLiab":"",)"
                 R"("disEq":"596.1871525",)"
                 R"("eq":"0.0121475",)"
                 R"("eqUsd":"596.1871525",)"
                 R"("frozenBal":"0",)"
                 R"("interest":"",)"
                 R"("isoEq":"0",)"
                 R"("isoLiab":"",)"
                 R"("isoUpl":"0",)"
                 R"("liab":"",)"
                 R"("maxLoan":"",)"
                 R"("mgnRatio":"",)"
                 R"("notionalLever":"0",)"
                 R"("ordFrozen":"0",)"
                 R"("stgyEq":"0",)"
                 R"("twap":"0",)"
                 R"("uTime":"1640088676388",)"
                 R"("upl":"0")"
                 R"(},{)"
                 R"("availBal":"",)"
                 R"("availEq":"0.00005916",)"
                 R"("cashBal":"0.00005916",)"
                 R"("ccy":"EOS",)"
                 R"("coinUsdPrice":"3.3476",)"
                 R"("crossLiab":"",)"
                 R"("disEq":"0.0001881418152",)"
                 R"("eq":"0.00005916",)"
                 R"("eqUsd":"0.000198044016",)"
                 R"("frozenBal":"0",)"
                 R"("interest":"",)"
                 R"("isoEq":"0",)"
                 R"("isoLiab":"",)"
                 R"("isoUpl":"0",)"
                 R"("liab":"",)"
                 R"("maxLoan":"",)"
                 R"("mgnRatio":"",)"
                 R"("notionalLever":"0",)"
                 R"("ordFrozen":"0",)"
                 R"("stgyEq":"0",)"
                 R"("twap":"0",)"
                 R"("uTime":"1624937815179",)"
                 R"("upl":"0")"
                 R"(},{)"
                 R"("availBal":"",)"
                 R"("availEq":"0.00000635",)"
                 R"("cashBal":"0.00000635",)"
                 R"("ccy":"OMG",)"
                 R"("coinUsdPrice":"6.1338",)"
                 R"("crossLiab":"",)"
                 R"("disEq":"0.000031159704",)"
                 R"("eq":"0.00000635",)"
                 R"("eqUsd":"0.00003894963",)"
                 R"("frozenBal":"0",)"
                 R"("interest":"",)"
                 R"("isoEq":"0",)"
                 R"("isoLiab":"",)"
                 R"("isoUpl":"0",)"
                 R"("liab":"",)"
                 R"("maxLoan":"",)"
                 R"("mgnRatio":"",)"
                 R"("notionalLever":"0",)"
                 R"("ordFrozen":"0",)"
                 R"("stgyEq":"0",)"
                 R"("twap":"0",)"
                 R"("uTime":"1624937815126",)"
                 R"("upl":"0")"
                 R"(})"
                 R"(],)"
                 R"("imr":"",)"
                 R"("isoEq":"0",)"
                 R"("mgnRatio":"",)"
                 R"("mmr":"",)"
                 R"("notionalUsd":"",)"
                 R"("ordFroz":"",)"
                 R"("totalEq":"596.187389493646",)"
                 R"("uTime":"1640151696581")"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == protocol::json::Channel::ACCOUNT);
    CHECK(obj.arg.uid == "33594834598109184"sv);
    REQUIRE(std::size(obj.data) == 1);
    auto &item = obj.data[0];
    CHECK(std::empty(item.adj_eq));
    auto &details = item.details;
    CHECK(std::size(item.details) == 3);
    // d0
    auto &detail_0 = details[0];
    CHECK(std::isnan(detail_0.avail_bal) == true);
    CHECK(detail_0.avail_eq == 0.0121475_a);
    CHECK(detail_0.cash_bal == 0.0121475_a);
    CHECK(detail_0.ccy == "BTC"sv);
    CHECK(detail_0.coin_usd_price == 49079.0_a);
    CHECK(std::isnan(detail_0.cross_liab) == true);
    CHECK(detail_0.dis_eq == 596.1871525_a);
    CHECK(detail_0.eq == 0.0121475_a);
    CHECK(detail_0.eq_usd == 596.1871525_a);
    CHECK(detail_0.frozen_bal == 0.0_a);
    CHECK(std::isnan(detail_0.interest) == true);
    CHECK(detail_0.iso_eq == 0.0_a);
    CHECK(std::isnan(detail_0.iso_liab) == true);
    CHECK(detail_0.iso_upl == 0.0_a);
    CHECK(std::isnan(detail_0.liab) == true);
    CHECK(std::isnan(detail_0.max_loan) == true);
    CHECK(std::isnan(detail_0.mgn_ratio) == true);
    CHECK(detail_0.notional_lever == 0.0_a);
    CHECK(detail_0.ord_frozen == 0.0_a);
    CHECK(detail_0.stgy_eq == 0.0_a);
    CHECK(detail_0.twap == 0.0_a);
    CHECK(detail_0.u_time == 1640088676388ms);
    CHECK(detail_0.upl == 0.0_a);
    // d1
    // d2
    // ...
    CHECK(std::isnan(item.imr) == true);
    CHECK(item.iso_eq == 0.0_a);
    CHECK(std::isnan(item.mgn_ratio) == true);
    CHECK(std::isnan(item.mmr) == true);
    CHECK(std::isnan(item.notional_usd) == true);
    CHECK(std::isnan(item.ord_froz) == true);
    CHECK(item.total_eq == 596.187389493646_a);
    CHECK(item.u_time == 1640151696581ms);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
