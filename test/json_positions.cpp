/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Positions;

TEST_CASE("simple", "[json_positions]") {
  auto message =
      // R"({"arg":{"channel":"positions","instType":"ANY","uid":"33594834598109184"},"data":[]}")";
      R"({)"
      R"("arg":{)"
      R"("channel":"positions",)"
      R"("instType":"ANY",)"
      R"("uid":"187956862690435072")"
      R"(},)"
      R"("data":[{)"
      R"("adl":"1",)"
      R"("availPos":"",)"
      R"("avgPx":"43684",)"
      R"("baseBal":"",)"
      R"("cTime":"1644330337903",)"
      R"("ccy":"USDT",)"
      R"("deltaBS":"",)"
      R"("deltaPA":"",)"
      R"("gammaBS":"",)"
      R"("gammaPA":"",)"
      R"("imr":"43.69104315276841",)"
      R"("instId":"BTC-USDT-SWAP",)"
      R"("instType":"SWAP",)"
      R"("interest":"0",)"
      R"("last":"43692.1",)"
      R"("lever":"10",)"
      R"("liab":"",)"
      R"("liabCcy":"",)"
      R"("liqPx":"23739.456120525683",)"
      R"("margin":"",)"
      R"("markPx":"43691.043152768407",)"
      R"("mgnMode":"cross",)"
      R"("mgnRatio":"102.02149323145969",)"
      R"("mmr":"1.7476417261107364",)"
      R"("notionalUsd":"437.3735565851034",)"
      R"("optVal":"",)"
      R"("pTime":"1644330793928",)"
      R"("pos":"1",)"
      R"("posCcy":"",)"
      R"("posId":"325382268437037058",)"
      R"("posSide":"net",)"
      R"("quoteBal":"",)"
      R"("thetaBS":"",)"
      R"("thetaPA":"",)"
      R"("tradeId":"186646171",)"
      R"("uTime":"1644330337903",)"
      R"("upl":"0.0704315276840498",)"
      R"("uplRatio":"0.0016122957532283",)"
      R"("usdPx":"",)"
      R"("vegaBS":"",)"
      R"("vegaPA":"")"
      R"(})"
      R"(])"
      R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::POSITIONS);
    CHECK(obj.arg.inst_type == "ANY"sv);
    CHECK(obj.arg.uid == "187956862690435072"sv);
    auto &data = obj.data;
    REQUIRE(std::size(data) == 1);
    auto &data_0 = data[0];
    CHECK(data_0.adl == 1);
    CHECK(std::isnan(data_0.avail_pos) == true);
    CHECK(data_0.avg_px == 43684.0_a);
    CHECK(std::isnan(data_0.base_bal) == true);
    CHECK(data_0.c_time == 1644330337903ms);
    CHECK(data_0.ccy == "USDT"sv);
    CHECK(std::isnan(data_0.delta_bs) == true);
    CHECK(std::isnan(data_0.delta_pa) == true);
    CHECK(std::isnan(data_0.gamma_bs) == true);
    CHECK(std::isnan(data_0.gamma_pa) == true);
    CHECK(data_0.imr == 43.69104315276841_a);
    CHECK(data_0.inst_id == "BTC-USDT-SWAP"sv);
    CHECK(data_0.inst_type == json::InstrumentType::SWAP);
    CHECK(data_0.interest == 0.0_a);
    CHECK(data_0.last == 43692.1_a);
    CHECK(data_0.lever == 10.0_a);
    CHECK(std::isnan(data_0.liab) == true);
    CHECK(std::empty(data_0.liab_ccy));
    CHECK(data_0.liq_px == 23739.456120525683_a);
    CHECK(std::isnan(data_0.margin) == true);
    CHECK(data_0.mark_px == 43691.043152768407_a);
    CHECK(data_0.mgn_mode == json::MarginMode::CROSS);
    CHECK(data_0.mgn_ratio == 102.02149323145969_a);
    CHECK(data_0.mmr == 1.7476417261107364_a);
    CHECK(data_0.notional_usd == 437.3735565851034_a);
    CHECK(std::isnan(data_0.opt_val) == true);
    CHECK(data_0.p_time == 1644330793928ms);
    CHECK(data_0.pos == 1.0_a);
    CHECK(std::empty(data_0.pos_ccy));
    CHECK(data_0.pos_id == "325382268437037058"sv);
    CHECK(data_0.pos_side == json::PositionSide::NET);
    CHECK(std::isnan(data_0.quote_bal) == true);
    CHECK(std::isnan(data_0.theta_bs) == true);
    CHECK(std::isnan(data_0.theta_pa) == true);
    CHECK(data_0.trade_id == "186646171"sv);
    CHECK(data_0.u_time == 1644330337903ms);
    CHECK(data_0.upl == 0.0704315276840498_a);
    CHECK(data_0.upl_ratio == 0.0016122957532283_a);
    CHECK(std::isnan(data_0.usd_px) == true);
    CHECK(std::isnan(data_0.vega_bs) == true);
    CHECK(std::isnan(data_0.vega_pa) == true);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
