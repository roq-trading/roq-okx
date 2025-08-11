/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_balance_and_position_parser", "[json_balance_and_position]") {
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
    void operator()(Trace<json::BalanceAndPosition> const &event) override {
      ++count_;
      auto &[trace_info, balance_and_position] = event;
      auto &bal_data = balance_and_position.bal_data;
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
      CHECK(balance_and_position.event_type == "snapshot"sv);
      CHECK(balance_and_position.p_time == 1640151123380ms);
      auto &pos_data = balance_and_position.pos_data;
      REQUIRE(std::size(pos_data) == 0);
    }
    void operator()(Trace<json::Positions> const &) override { FAIL(); }
    void operator()(Trace<json::Orders> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::Candle> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}

TEST_CASE("json_balance_and_position_sample_2", "[json_balance_and_position]") {
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
    void operator()(Trace<json::BalanceAndPosition> const &event) override {
      ++count_;
      auto &[trace_info, balance_and_position] = event;
      auto &bal_data = balance_and_position.bal_data;
      REQUIRE(std::size(bal_data) == 1);
      auto &bal_0 = bal_data[0];
      CHECK(bal_0.cash_bal == 200.5137143201668551_a);
      CHECK(bal_0.ccy == "USDT"sv);
      CHECK(bal_0.u_time == 1644330337903ms);
      CHECK(balance_and_position.event_type == "snapshot"sv);
      CHECK(balance_and_position.p_time == 1644330371092ms);
      auto &pos_data = balance_and_position.pos_data;
      REQUIRE(std::size(pos_data) == 1);
      auto &pos_0 = pos_data[0];
      CHECK(pos_0.avg_px == 43684.0_a);
      CHECK(std::isnan(pos_0.base_bal) == true);
      CHECK(pos_0.ccy == "USDT"sv);
      CHECK(pos_0.inst_id == "BTC-USDT-SWAP"sv);
      CHECK(pos_0.inst_type == json::InstrumentType::SWAP);
      CHECK(pos_0.mgn_mode == json::MarginMode::CROSS);
      CHECK(pos_0.pos == 1.0_a);
      CHECK(std::empty(pos_0.pos_ccy));
      CHECK(pos_0.pos_id == "325382268437037058"sv);
      CHECK(pos_0.pos_side == json::PositionSide::NET);
      CHECK(std::isnan(pos_0.quote_bal) == true);
      CHECK(pos_0.trade_id == "186646171"sv);
      CHECK(pos_0.u_time == 1644330337903ms);
    }
    void operator()(Trace<json::Positions> const &) override { FAIL(); }
    void operator()(Trace<json::Orders> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::Candle> const &) override { FAIL(); }

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
