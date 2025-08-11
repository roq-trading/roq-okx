/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_instruments_parser", "[json_instruments]") {
  // note! truncated
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"instruments",)"
                 R"("instType":"SPOT")"
                 R"(},)"
                 R"("data":[{)"
                 R"("alias":"",)"
                 R"("baseCcy":"BCD",)"
                 R"("category":"2",)"
                 R"("ctMult":"",)"
                 R"("ctType":"",)"
                 R"("ctVal":"",)"
                 R"("ctValCcy":"",)"
                 R"("expTime":"",)"
                 R"("instId":"BCD-BTC",)"
                 R"("instType":"SPOT",)"
                 R"("lever":"",)"
                 R"("listTime":"",)"
                 R"("lotSz":"0.0001",)"
                 R"("minSz":"1",)"
                 R"("optType":"",)"
                 R"("quoteCcy":"BTC",)"
                 R"("settleCcy":"",)"
                 R"("state":"live",)"
                 R"("stk":"",)"
                 R"("tickSz":"0.0000001",)"
                 R"("uly":"")"
                 R"(},{)"
                 R"("alias":"",)"
                 R"("baseCcy":"MDT",)"
                 R"("category":"2",)"
                 R"("ctMult":"",)"
                 R"("ctType":"",)"
                 R"("ctVal":"",)"
                 R"("ctValCcy":"",)"
                 R"("expTime":"",)"
                 R"("instId":"MDT-USDT",)"
                 R"("instType":"SPOT",)"
                 R"("lever":"",)"
                 R"("listTime":"",)"
                 R"("lotSz":"0.000001",)"
                 R"("minSz":"10",)"
                 R"("optType":"",)"
                 R"("quoteCcy":"USDT",)"
                 R"("settleCcy":"",)"
                 R"("state":"live",)"
                 R"("stk":"",)"
                 R"("tickSz":"0.00001",)"
                 R"("uly":"")"
                 R"(},{)"
                 R"("alias":"",)"
                 R"("baseCcy":"API3",)"
                 R"("category":"3",)"
                 R"("ctMult":"",)"
                 R"("ctType":"",)"
                 R"("ctVal":"",)"
                 R"("ctValCcy":"",)"
                 R"("expTime":"",)"
                 R"("instId":"API3-OKB",)"
                 R"("instType":"SPOT",)"
                 R"("lever":"",)"
                 R"("listTime":"",)"
                 R"("lotSz":"0.000001",)"
                 R"("minSz":"1",)"
                 R"("optType":"",)"
                 R"("quoteCcy":"OKB",)"
                 R"("settleCcy":"",)"
                 R"("state":"live",)"
                 R"("stk":"",)"
                 R"("tickSz":"0.00001",)"
                 R"("uly":"")"
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
    void operator()(Trace<json::Instruments> const &event) override {
      ++count_;
      auto &[trace_info, instruments] = event;
      auto &data = instruments.data;
      REQUIRE(std::size(data) == 3);
      // data_0
      auto &data_0 = data[0];
      CHECK(std::empty(data_0.alias));
      CHECK(data_0.base_ccy == "BCD"sv);
      CHECK(data_0.category == "2"sv);
      CHECK(std::isnan(data_0.ct_mult) == true);
      CHECK(data_0.ct_type == json::ContractType::UNDEFINED_INTERNAL);
      CHECK(std::isnan(data_0.ct_val) == true);
      CHECK(std::empty(data_0.ct_val_ccy));
      CHECK(data_0.exp_time == 0ms);
      CHECK(data_0.inst_id == "BCD-BTC"sv);
      CHECK(data_0.inst_type == json::InstrumentType::SPOT);
      CHECK(std::isnan(data_0.lever) == true);
      CHECK(data_0.list_time == 0ms);
      CHECK(data_0.lot_sz == 0.0001_a);
      CHECK(data_0.min_sz == 1.0_a);
      CHECK(data_0.opt_type == json::OptionType::UNDEFINED_INTERNAL);
      CHECK(data_0.quote_ccy == "BTC"sv);
      CHECK(std::empty(data_0.settle_ccy));
      CHECK(data_0.state == json::InstrumentState::LIVE);
      CHECK(std::isnan(data_0.stk) == true);
      CHECK(data_0.tick_sz == 0.0000001_a);
      CHECK(std::empty(data_0.uly));
    }
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
