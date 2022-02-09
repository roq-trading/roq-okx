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

TEST(json_balance_and_position, parser) {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"balance_and_position",)"
                 R"("uid":"33594834598109184"},)"
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
    void operator()(server::Trace<json::BalanceAndPosition> const &event) override {
      ++count_;
      auto &[trace_info, balance_and_position] = event;
      auto &bal_data = balance_and_position.bal_data;
      ASSERT_EQ(std::size(bal_data), 3);
      auto &b0 = bal_data[0];
      EXPECT_DOUBLE_EQ(b0.cash_bal, 0.0121475);
      EXPECT_EQ(b0.ccy, "BTC"sv);
      EXPECT_EQ(b0.u_time, 1640088676388ms);
      auto &b1 = bal_data[1];
      EXPECT_DOUBLE_EQ(b1.cash_bal, 0.00000635);
      EXPECT_EQ(b1.ccy, "OMG"sv);
      EXPECT_EQ(b1.u_time, 1624937815126ms);
      auto &b2 = bal_data[2];
      EXPECT_DOUBLE_EQ(b2.cash_bal, 0.00005916);
      EXPECT_EQ(b2.ccy, "EOS"sv);
      EXPECT_EQ(b2.u_time, 1624937815179ms);
      EXPECT_EQ(balance_and_position.event_type, "snapshot"sv);
      EXPECT_EQ(balance_and_position.p_time, 1640151123380ms);
      auto &pos_data = balance_and_position.pos_data;
      ASSERT_EQ(std::size(pos_data), 0);
    }
    void operator()(server::Trace<json::Positions> const &) override { FAIL(); }
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
  EXPECT_TRUE(res);
  EXPECT_EQ(handler.get_count(), 1);
}

TEST(json_balance_and_position, sample_2) {
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
    void operator()(server::Trace<json::BalanceAndPosition> const &event) override {
      ++count_;
      auto &[trace_info, balance_and_position] = event;
      auto &bal_data = balance_and_position.bal_data;
      ASSERT_EQ(std::size(bal_data), 1);
      auto &b0 = bal_data[0];
      EXPECT_DOUBLE_EQ(b0.cash_bal, 200.5137143201668551);
      EXPECT_EQ(b0.ccy, "USDT"sv);
      EXPECT_EQ(b0.u_time, 1644330337903ms);
      EXPECT_EQ(balance_and_position.event_type, "snapshot"sv);
      EXPECT_EQ(balance_and_position.p_time, 1644330371092ms);
      auto &pos_data = balance_and_position.pos_data;
      ASSERT_EQ(std::size(pos_data), 1);
      auto &p0 = pos_data[0];
      EXPECT_DOUBLE_EQ(p0.avg_px, 43684.0);
      EXPECT_TRUE(std::isnan(p0.base_bal));
      EXPECT_EQ(p0.ccy, "USDT"sv);
      EXPECT_EQ(p0.inst_id, "BTC-USDT-SWAP"sv);
      EXPECT_EQ(p0.inst_type, json::InstrumentType::SWAP);
      EXPECT_EQ(p0.mgn_mode, json::MarginMode::CROSS);
      EXPECT_DOUBLE_EQ(p0.pos, 1.0);
      EXPECT_EQ(p0.pos_ccy, ""sv);
      EXPECT_EQ(p0.pos_id, "325382268437037058"sv);
      EXPECT_EQ(p0.pos_side, json::PositionSide::NET);
      EXPECT_TRUE(std::isnan(p0.quote_bal));
      EXPECT_EQ(p0.trade_id, "186646171"sv);
      EXPECT_EQ(p0.u_time, 1644330337903ms);
    }
    void operator()(server::Trace<json::Positions> const &) override { FAIL(); }
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
  EXPECT_TRUE(res);
  EXPECT_EQ(handler.get_count(), 1);
}
