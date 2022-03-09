/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <catch2/catch.hpp>

#include "roq/core/json/parser.hpp"

#include "roq/okx/json/parser.hpp"

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
    void operator()(server::Trace<json::Error> const &) override { FAIL(); }
    void operator()(server::Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(server::Trace<json::Status> const &) override { FAIL(); }
    void operator()(server::Trace<json::Instruments> const &event) override {
      ++count_;
      auto &[trace_info, instruments] = event;
      auto &data = instruments.data;
      REQUIRE(std::size(data) == 3);
      // d0
      auto &d0 = data[0];
      CHECK(d0.alias == ""sv);
      CHECK(d0.base_ccy == "BCD"sv);
      CHECK(d0.category == "2"sv);
      CHECK(std::isnan(d0.ct_mult) == true);
      CHECK(d0.ct_type == json::ContractType::UNDEFINED);
      CHECK(std::isnan(d0.ct_val) == true);
      CHECK(d0.ct_val_ccy == ""sv);
      CHECK(d0.exp_time == 0ms);
      CHECK(d0.inst_id == "BCD-BTC"sv);
      CHECK(d0.inst_type == json::InstrumentType::SPOT);
      CHECK(std::isnan(d0.lever) == true);
      CHECK(d0.list_time == 0ms);
      CHECK(d0.lot_sz == 0.0001_a);
      CHECK(d0.min_sz == 1.0_a);
      CHECK(d0.opt_type == json::OptionType::UNDEFINED);
      CHECK(d0.quote_ccy == "BTC"sv);
      CHECK(d0.settle_ccy == ""sv);
      CHECK(d0.state == json::State::LIVE);
      CHECK(std::isnan(d0.stk) == true);
      CHECK(d0.tick_sz == 0.0000001_a);
      CHECK(d0.uly == ""sv);
    }
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
