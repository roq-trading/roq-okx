/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okx/json/parser.h"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

namespace {
auto create_trace_info() {
  return server::TraceInfo{
      .source_receive_time = {},
      .origin_create_time = {},
      .origin_create_time_utc = {},
  };
}
}  // namespace

TEST(json_orders, download_empty) {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[],)"
                 R"("msg":"")"
                 R"(})";
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Orders>(message, buffer_);
  EXPECT_EQ(obj.code, 0);
  ASSERT_EQ(std::size(obj.data), 0);
  EXPECT_EQ(obj.msg, ""sv);
}

TEST(json_orders, download) {
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
  core::Buffer buffer(8192);
  core::json::Buffer buffer_(buffer);
  auto obj = core::json::Parser::create<json::Orders>(message, buffer_);
  EXPECT_EQ(obj.code, 0);
  ASSERT_EQ(std::size(obj.data), 1);
  // XXX HANS
  EXPECT_EQ(obj.msg, ""sv);
}

TEST(json_orders, parser) {
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
    void operator()(server::Trace<json::Positions> const &) override { FAIL(); }
    void operator()(server::Trace<json::Orders> const &event) override {
      ++count_;
      auto &[trace_info, orders] = event;
      // XXX HANS
    }
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
  EXPECT_TRUE(res);
  EXPECT_EQ(handler.get_count(), 1);
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
