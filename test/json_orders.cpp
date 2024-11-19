/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_orders_download_empty", "[json_orders]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[],)"
                 R"("msg":"")"
                 R"(})";
  std::vector<std::byte> buffer(8192);
  json::Orders obj{message, buffer};
  CHECK(obj.code == 0);
  REQUIRE(std::size(obj.data) == 0);
  CHECK(obj.msg == ""sv);
}

TEST_CASE("json_orders_download", "[json_orders]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("accFillSz":"0",)"
                 R"("avgPx":"",)"
                 R"("cTime":"1640182694746",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"abcABC125",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillPx":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("instType":"FUTURES",)"
                 R"("lever":"10",)"
                 R"("ordId":"393890002618445825",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"long",)"
                 R"("px":"39919.4",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"BTC",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"live",)"
                 R"("sz":"1",)"
                 R"("tag":"",)"
                 R"("tdMode":"isolated",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1640182694746")"
                 R"(})"
                 R"(],)"
                 R"("msg":"")"
                 R"(})";
  std::vector<std::byte> buffer(8192);
  json::Orders obj{message, buffer};
  CHECK(obj.code == 0);
  REQUIRE(std::size(obj.data) == 1);
  // XXX HANS
  CHECK(obj.msg == ""sv);
}

TEST_CASE("json_orders_parser", "[json_orders]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"orders",)"
                 R"("instType":"ANY",)"
                 R"("uid":"33594834598109184")"
                 R"(},)"
                 R"("data":[{)"
                 R"("accFillSz":"0",)"
                 R"("amendResult":"",)"
                 R"("avgPx":"0",)"
                 R"("cTime":"1640182694746",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"abcABC125",)"
                 R"("code":"0",)"
                 R"("execType":"",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillFee":"0",)"
                 R"("fillFeeCcy":"",)"
                 R"("fillNotionalUsd":"",)"
                 R"("fillPx":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("instType":"FUTURES",)"
                 R"("lever":"10",)"
                 R"("msg":"",)"
                 R"("notionalUsd":"100.0",)"
                 R"("ordId":"393890002618445825",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"long",)"
                 R"("px":"39919.4",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"BTC",)"
                 R"("reduceOnly":"false",)"
                 R"("reqId":"",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"live",)"
                 R"("sz":"1",)"
                 R"("tag":"",)"
                 R"("tdMode":"isolated",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1640182694746")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(Trace<json::Error> const &) override { FAIL(); }
    void operator()(Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Status> const &) override { FAIL(); }
    void operator()(Trace<json::Instruments> const &) override { FAIL(); }
    void operator()(Trace<json::EstimatedPrice> const &) override { FAIL(); }
    void operator()(Trace<json::PriceLimit> const &) override { FAIL(); }
    void operator()(Trace<json::MarkPrice> const &) override { FAIL(); }
    void operator()(Trace<json::Tickers> const &) override { FAIL(); }
    void operator()(Trace<json::Trades> const &) override { FAIL(); }
    void operator()(Trace<json::BboTbt> const &, [[maybe_unused]] std::string_view const &inst_id) override { FAIL(); }
    void operator()(Trace<json::BooksL2Tbt> const &, [[maybe_unused]] std::string_view const &inst_id, json::Action) override { FAIL(); }
    void operator()(Trace<json::IndexTickers> const &) override { FAIL(); }
    void operator()(Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(Trace<json::ChannelConnCount> const &) override { FAIL(); }
    void operator()(Trace<json::Login> const &) override { FAIL(); }
    void operator()(Trace<json::Account> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(Trace<json::Positions> const &) override { FAIL(); }
    void operator()(Trace<json::Orders> const &event) override {
      ++count_;
      auto &[trace_info, orders] = event;
      auto &data = orders.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.acc_fill_sz == 0.0_a);
      CHECK(d0.amend_result == 0);
      CHECK(d0.avg_px == 0.0_a);
      CHECK(d0.c_time == 1640182694746ms);
      CHECK(d0.category == json::Category::NORMAL);
      CHECK(d0.ccy == ""sv);
      CHECK(d0.cl_ord_id == "abcABC125"sv);
      CHECK(d0.code == 0);
      CHECK(d0.exec_type == json::OrderFlowType::UNDEFINED__);
      CHECK(d0.fee == 0.0_a);
      CHECK(d0.fee_ccy == "BTC"sv);
      CHECK(d0.fill_fee == 0.0_a);
      CHECK(d0.fill_fee_ccy == ""sv);
      CHECK(d0.fill_notional_usd == ""sv);
      CHECK(std::isnan(d0.fill_px) == true);
      CHECK(d0.fill_sz == 0.0_a);
      CHECK(d0.fill_time == 0ms);
      CHECK(d0.inst_id == "BTC-USD-220325"sv);
      CHECK(d0.inst_type == json::InstrumentType::FUTURES);
      CHECK(d0.lever == 10.0_a);
      CHECK(d0.msg == ""sv);
      CHECK(d0.notional_usd == 100.0_a);
      CHECK(d0.ord_id == "393890002618445825"sv);
      CHECK(d0.ord_type == json::OrderType::LIMIT);
      CHECK(d0.pnl == 0.0_a);
      CHECK(d0.pos_side == json::PositionSide::LONG);
      CHECK(d0.px == 39919.4_a);
      CHECK(d0.rebate == 0.0_a);
      CHECK(d0.rebate_ccy == "BTC"sv);
      CHECK(d0.reduce_only == false);
      CHECK(d0.req_id == ""sv);
      CHECK(d0.side == json::Side::BUY);
      CHECK(std::isnan(d0.sl_ord_px) == true);
      CHECK(std::isnan(d0.sl_trigger_px) == true);
      CHECK(d0.sl_trigger_px_type == json::TriggerPriceType::UNDEFINED__);
      CHECK(d0.source == ""sv);
      CHECK(d0.state == json::OrderState::LIVE);
      CHECK(d0.sz == 1.0_a);
      CHECK(d0.tag == ""sv);
      CHECK(d0.td_mode == json::TradeMode::ISOLATED);
      CHECK(d0.tgt_ccy == ""sv);
      CHECK(std::isnan(d0.tp_ord_px) == true);
      CHECK(std::isnan(d0.tp_trigger_px) == true);
      CHECK(d0.tp_trigger_px_type == json::TriggerPriceType::UNDEFINED__);
      CHECK(d0.trade_id == ""sv);
      CHECK(d0.u_time == 1640182694746ms);
    }
    void operator()(Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}

TEST_CASE("json_orders_parser_2", "[json_orders]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"orders",)"
                 R"("instType":"ANY",)"
                 R"("uid":"474527194543234199")"
                 R"(},)"
                 R"("data":[{)"
                 R"("accFillSz":"0",)"
                 R"("algoClOrdId":"",)"
                 R"("algoId":"",)"
                 R"("amendResult":"",)"
                 R"("amendSource":"",)"
                 R"("attachAlgoClOrdId":"",)"
                 R"("avgPx":"0",)"
                 R"("cTime":"1699330197524",)"
                 R"("cancelSource":"",)"
                 R"("category":"normal",)"
                 R"("ccy":"",)"
                 R"("clOrdId":"X4BAEA772BXLWAABAAAAAAAA",)"
                 R"("code":"0",)"
                 R"("execType":"",)"
                 R"("fee":"0",)"
                 R"("feeCcy":"BTC",)"
                 R"("fillFee":"0",)"
                 R"("fillFeeCcy":"",)"
                 R"("fillFwdPx":"",)"
                 R"("fillMarkPx":"",)"
                 R"("fillMarkVol":"",)"
                 R"("fillNotionalUsd":"",)"
                 R"("fillPnl":"0",)"
                 R"("fillPx":"",)"
                 R"("fillPxUsd":"",)"
                 R"("fillPxVol":"",)"
                 R"("fillSz":"0",)"
                 R"("fillTime":"",)"
                 R"("instId":"BTC-USDT",)"
                 R"("instType":"SPOT",)"
                 R"("lastPx":"34910",)"
                 R"("lever":"5",)"
                 R"("msg":"",)"
                 R"("notionalUsd":"3.4517940000000005",)"
                 R"("ordId":"641972610108518410",)"
                 R"("ordType":"limit",)"
                 R"("pnl":"0",)"
                 R"("posSide":"",)"
                 R"("px":"34500",)"
                 R"("pxType":"",)"
                 R"("pxUsd":"",)"
                 R"("pxVol":"",)"
                 R"("quickMgnType":"",)"
                 R"("rebate":"0",)"
                 R"("rebateCcy":"USDT",)"
                 R"("reduceOnly":"false",)"
                 R"("reqId":"",)"
                 R"("side":"buy",)"
                 R"("slOrdPx":"",)"
                 R"("slTriggerPx":"",)"
                 R"("slTriggerPxType":"",)"
                 R"("source":"",)"
                 R"("state":"live",)"
                 R"("stpId":"",)"
                 R"("stpMode":"",)"
                 R"("sz":"0.0001",)"
                 R"("tag":"",)"
                 R"("tdMode":"cross",)"
                 R"("tgtCcy":"",)"
                 R"("tpOrdPx":"",)"
                 R"("tpTriggerPx":"",)"
                 R"("tpTriggerPxType":"",)"
                 R"("tradeId":"",)"
                 R"("uTime":"1699330197524")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(Trace<json::Error> const &) override { FAIL(); }
    void operator()(Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Status> const &) override { FAIL(); }
    void operator()(Trace<json::Instruments> const &) override { FAIL(); }
    void operator()(Trace<json::EstimatedPrice> const &) override { FAIL(); }
    void operator()(Trace<json::PriceLimit> const &) override { FAIL(); }
    void operator()(Trace<json::MarkPrice> const &) override { FAIL(); }
    void operator()(Trace<json::Tickers> const &) override { FAIL(); }
    void operator()(Trace<json::Trades> const &) override { FAIL(); }
    void operator()(Trace<json::BboTbt> const &, [[maybe_unused]] std::string_view const &inst_id) override { FAIL(); }
    void operator()(Trace<json::BooksL2Tbt> const &, [[maybe_unused]] std::string_view const &inst_id, json::Action) override { FAIL(); }
    void operator()(Trace<json::IndexTickers> const &) override { FAIL(); }
    void operator()(Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(Trace<json::ChannelConnCount> const &) override { FAIL(); }
    void operator()(Trace<json::Login> const &) override { FAIL(); }
    void operator()(Trace<json::Account> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(Trace<json::Positions> const &) override { FAIL(); }
    void operator()(Trace<json::Orders> const &) override {
      ++count_;
      // XXX TODO check fields
    }
    void operator()(Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}

/*
create order
{"arg":{"channel":"orders","instType":"ANY","uid":"33594834598109184"},"data":[{"accFillSz":"0","amendResult":"","avgPx":"0","cTime":"1640194677237","category":"normal","ccy":"","clOrdId":"3MAAF2IDAAAQAAGSKMZCT5A3","code":"0","execType":"","fee":"0","feeCcy":"BTC","fillFee":"0","fillFeeCcy":"","fillNotionalUsd":"","fillPx":"","fillSz":"0","fillTime":"","instId":"BTC-USD-220325","instType":"FUTURES","lever":"10","msg":"","notionalUsd":"100.0","ordId":"393940260828377089","ordType":"limit","pnl":"0","posSide":"long","px":"40030.2","rebate":"0","rebateCcy":"BTC","reduceOnly":"false","reqId":"","side":"buy","slOrdPx":"","slTriggerPx":"","slTriggerPxType":"","source":"","state":"live","sz":"1","tag":"","tdMode":"isolated","tgtCcy":"","tpOrdPx":"","tpTriggerPx":"","tpTriggerPxType":"","tradeId":"","uTime":"1640194677237"}]}
// ->
{"arg":{"channel":"account","uid":"33594834598109184"},"data":[{"adjEq":"","details":[{"availBal":"","availEq":"0.0103939932844976","cashBal":"0.0121475","ccy":"BTC","coinUsdPrice":"48862.2","crossLiab":"","disEq":"593.5535745","eq":"0.0121475","eqUsd":"593.5535745","frozenBal":"0.0017535067155024","interest":"","isoEq":"0","isoLiab":"","isoUpl":"0","liab":"","maxLoan":"","mgnRatio":"","notionalLever":"0","ordFrozen":"0.0017447828014949","stgyEq":"0","twap":"0","uTime":"1640088676432","upl":"0"}],"imr":"","isoEq":"0","mgnRatio":"","mmr":"","notionalUsd":"","ordFroz":"","totalEq":"593.553816464492","uTime":"1640194677352"}]}
modify order
{"arg":{"channel":"orders","instType":"ANY","uid":"33594834598109184"},"data":[{"accFillSz":"0","amendResult":"0","avgPx":"0","cTime":"1640194677237","category":"normal","ccy":"","clOrdId":"3MAAF2IDAAAQAAGSKMZCT5A3","code":"0","execType":"","fee":"0","feeCcy":"BTC","fillFee":"0","fillFeeCcy":"","fillNotionalUsd":"","fillPx":"","fillSz":"0","fillTime":"","instId":"BTC-USD-220325","instType":"FUTURES","lever":"10","msg":"","notionalUsd":"100.0","ordId":"393940260828377089","ordType":"limit","pnl":"0","posSide":"long","px":"30036.7","rebate":"0","rebateCcy":"BTC","reduceOnly":"false","reqId":"","side":"buy","slOrdPx":"","slTriggerPx":"","slTriggerPxType":"","source":"","state":"live","sz":"1","tag":"","tdMode":"isolated","tgtCcy":"","tpOrdPx":"","tpTriggerPx":"","tpTriggerPxType":"","tradeId":"","uTime":"1640194680444"}]}
cancel order
{"arg":{"channel":"orders","instType":"ANY","uid":"33594834598109184"},"data":[{"accFillSz":"0","amendResult":"","avgPx":"0","cTime":"1640194677237","category":"normal","ccy":"","clOrdId":"3MAAF2IDAAAQAAGSKMZCT5A3","code":"0","execType":"","fee":"0","feeCcy":"BTC","fillFee":"0","fillFeeCcy":"","fillNotionalUsd":"","fillPx":"","fillSz":"0","fillTime":"","instId":"BTC-USD-220325","instType":"FUTURES","lever":"10","msg":"","notionalUsd":"100.0","ordId":"393940260828377089","ordType":"limit","pnl":"0","posSide":"long","px":"30036.7","rebate":"0","rebateCcy":"BTC","reduceOnly":"false","reqId":"","side":"buy","slOrdPx":"","slTriggerPx":"","slTriggerPxType":"","source":"","state":"canceled","sz":"1","tag":"","tdMode":"isolated","tgtCcy":"","tpOrdPx":"","tpTriggerPx":"","tpTriggerPxType":"","tradeId":"","uTime":"1640194683650"}]}"
*/

/*
 * MAKER
 orders={code=0, msg="", arg={channel=ORDERS, inst_id="", inst_type="ANY",
 uid="187956862690435072"}, data=[{acc_fill_sz=1, amend_result=0, avg_px=43232.7, category=NORMAL,
 ccy="", cl_ord_id="IIAAF2QDAAAQAAA3HSY5DNY7", code=0, c_time=1644333557740ms, exec_type=MAKER,
 fee_ccy="USDT", fee=-0.08646540000000003, fill_fee_ccy="USDT", fill_fee=-0.08646540000000003,
 fill_notional_usd="432.57800000000003", fill_px=43232.7, fill_sz=1, fill_time=1644333559434ms,
 inst_id="BTC-USDT-SWAP", inst_type=SWAP, lever=10, msg="", notional_usd="432.57800000000003",
 ord_id="411299983877636110", ord_type=LIMIT, pnl=0.11800000000000002, pos_side=NET, px=43232.7,
 rebate_ccy="USDT", rebate=0, reduce_only=false, req_id="", side=SELL, sl_ord_px=nan,
 sl_trigger_px=nan, sl_trigger_px_type=UNDEFINED, source="", state=FILLED, sz=1, tag="",
 td_mode=CROSS, tgt_ccy="", tp_ord_px=nan, tp_trigger_px=nan, tp_trigger_px_type=UNDEFINED,
 trade_id="186781262", u_time=1644333559437ms}]}

 TAKER orders={code=0, msg="", arg={channel=ORDERS,
 inst_id="", inst_type="ANY", uid="187956862690435072"}, data=[{acc_fill_sz=1, amend_result=0,
 avg_px=43220.9, category=NORMAL, ccy="", cl_ord_id="HYAAF2IDAAAQAAF7FBQ45NY7", code=0,
 c_time=1644333502163ms, exec_type=TAKER, fee_ccy="USDT", fee=-0.2161045, fill_fee_ccy="USDT",
 fill_fee=-0.2161045, fill_notional_usd="432.398", fill_px=43220.9, fill_sz=1,
 fill_time=1644333532480ms, inst_id="BTC-USDT-SWAP", inst_type=SWAP, lever=10, msg="",
 notional_usd="432.398", ord_id="411299750770802690", ord_type=LIMIT, pnl=0, pos_side=NET, px=43221,
 rebate_ccy="USDT", rebate=0, reduce_only=false, req_id="", side=BUY, sl_ord_px=nan,
 sl_trigger_px=nan, sl_trigger_px_type=UNDEFINED, source="", state=FILLED, sz=1, tag="",
 td_mode=CROSS, tgt_ccy="", tp_ord_px=nan, tp_trigger_px=nan, tp_trigger_px_type=UNDEFINED,
 trade_id="186779689", u_time=1644333532483ms}]}
*/

/* download
 * {"code":"0","data":[{"accFillSz":"0","avgPx":"","cTime":"1644376847070","category":"normal","ccy":"","clOrdId":"RMAAF2QDAAAQAAAA6LX6LQI7","fee":"0","feeCcy":"USDT","fillPx":"","fillSz":"0","fillTime":"","instId":"BTC-USDT-SWAP","instType":"SWAP","lever":"10","ordId":"411481552487612418","ordType":"limit","pnl":"0","posSide":"net","px":"33379","rebate":"0","rebateCcy":"USDT","side":"buy","slOrdPx":"","slTriggerPx":"","slTriggerPxType":"","source":"","state":"live","sz":"1","tag":"","tdMode":"cross","tgtCcy":"","tpOrdPx":"","tpTriggerPx":"","tpTriggerPxType":"","tradeId":"","uTime":"1644376847070"}],"msg":""}
 */
