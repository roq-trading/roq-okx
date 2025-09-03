/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_account_parser", "[json_account]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"account",)"
                 R"("uid":"33594834598109184"},)"
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
    void operator()(Trace<json::Account> const &event) override {
      ++count_;
      auto &[trace_info, account] = event;
      CHECK(std::empty(account.adj_eq));
      auto &details = account.details;
      CHECK(std::size(account.details) == 3);
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
      CHECK(std::isnan(account.imr) == true);
      CHECK(account.iso_eq == 0.0_a);
      CHECK(std::isnan(account.mgn_ratio) == true);
      CHECK(std::isnan(account.mmr) == true);
      CHECK(std::isnan(account.notional_usd) == true);
      CHECK(std::isnan(account.ord_froz) == true);
      CHECK(account.total_eq == 596.187389493646_a);
      CHECK(account.u_time == 1640151696581ms);
    }
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
  core::json::BufferStack buffer{8192, 1};
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
