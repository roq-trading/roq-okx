/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <catch2/catch.hpp>

#include "roq/core/json/parser.h"

#include "roq/okx/json/parser.h"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

namespace {
auto create_trace_info() {
  return server::TraceInfo{
      .source_receive_time = {},
      .origin_create_time = {},
      .origin_create_time_utc = {},
  };
}
}  // namespace

TEST_CASE("json_positions_parser", "[json_positions]") {
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
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(server::Trace<json::Error> const &) override { FAIL(); }
    void operator()(server::Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Status> const &) override { FAIL(); }
    void operator()(server::Trace<json::Instruments> const &) override { FAIL(); }
    void operator()(server::Trace<json::EstimatedPrice> const &) override { FAIL(); }
    void operator()(server::Trace<json::PriceLimit> const &) override { FAIL(); }
    void operator()(server::Trace<json::MarkPrice> const &) override { FAIL(); }
    void operator()(server::Trace<json::Tickers> const &) override { FAIL(); }
    void operator()(server::Trace<json::Trades> const &) override { FAIL(); }
    void operator()(
        server::Trace<json::BooksL2Tbt> const &,
        [[maybe_unused]] const std::string_view &inst_id,
        json::Action) override {
      FAIL();
    }
    void operator()(server::Trace<json::IndexTickers> const &) override { FAIL(); }
    void operator()(server::Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(server::Trace<json::Login> const &) override { FAIL(); }
    void operator()(server::Trace<json::Account> const &) override { FAIL(); }
    void operator()(server::Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(server::Trace<json::Positions> const &event) override {
      ++count_;
      auto &[trace_info, positions] = event;
      auto &data = positions.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.adl == 1);
      CHECK(std::isnan(d0.avail_pos) == true);
      CHECK(d0.avg_px == 43684.0_a);
      CHECK(std::isnan(d0.base_bal) == true);
      CHECK(d0.c_time == 1644330337903ms);
      CHECK(d0.ccy == "USDT"sv);
      CHECK(std::isnan(d0.delta_bs) == true);
      CHECK(std::isnan(d0.delta_pa) == true);
      CHECK(std::isnan(d0.gamma_bs) == true);
      CHECK(std::isnan(d0.gamma_pa) == true);
      CHECK(d0.imr == 43.69104315276841_a);
      CHECK(d0.inst_id == "BTC-USDT-SWAP"sv);
      CHECK(d0.inst_type == json::InstrumentType::SWAP);
      CHECK(d0.interest == 0.0_a);
      CHECK(d0.last == 43692.1_a);
      CHECK(d0.lever == 10.0_a);
      CHECK(std::isnan(d0.liab) == true);
      CHECK(d0.liab_ccy == ""sv);
      CHECK(d0.liq_px == 23739.456120525683_a);
      CHECK(std::isnan(d0.margin) == true);
      CHECK(d0.mark_px == 43691.043152768407_a);
      CHECK(d0.mgn_mode == json::MarginMode::CROSS);
      CHECK(d0.mgn_ratio == 102.02149323145969_a);
      CHECK(d0.mmr == 1.7476417261107364_a);
      CHECK(d0.notional_usd == 437.3735565851034_a);
      CHECK(std::isnan(d0.opt_val) == true);
      CHECK(d0.p_time == 1644330793928ms);
      CHECK(d0.pos == 1.0_a);
      CHECK(d0.pos_ccy == ""sv);
      CHECK(d0.pos_id == "325382268437037058"sv);
      CHECK(d0.pos_side == json::PositionSide::NET);
      CHECK(std::isnan(d0.quote_bal) == true);
      CHECK(std::isnan(d0.theta_bs) == true);
      CHECK(std::isnan(d0.theta_pa) == true);
      CHECK(d0.trade_id == "186646171"sv);
      CHECK(d0.u_time == 1644330337903ms);
      CHECK(d0.upl == 0.0704315276840498_a);
      CHECK(d0.upl_ratio == 0.0016122957532283_a);
      CHECK(std::isnan(d0.usd_px) == true);
      CHECK(std::isnan(d0.vega_bs) == true);
      CHECK(std::isnan(d0.vega_pa) == true);
    }
    void operator()(server::Trace<json::Orders> const &) override { FAIL(); }
    void operator()(server::Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::CancelOrderAck> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto trace_info = create_trace_info();
  auto res = json::Parser::dispatch(handler, message, buffer_, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
