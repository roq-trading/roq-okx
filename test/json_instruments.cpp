/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::Instruments;

// note! truncated
TEST_CASE("simple", "[json_instruments]") {
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
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::INSTRUMENTS);
    CHECK(obj.arg.inst_type == "SPOT"sv);
    REQUIRE(std::size(obj.data) == 3);
    // data_0
    auto &data_0 = obj.data[0];
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
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 2);
}
