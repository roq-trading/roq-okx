/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_index_tickers_parser", "[json_index_tickers]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"index-tickers",)"
                 R"("instId":"BTC-USDT")"
                 R"(},)"
                 R"("data":[{)"
                 R"("instId":"BTC-USDT",)"
                 R"("idxPx":"41756.7",)"
                 R"("open24h":"42427.3",)"
                 R"("high24h":"42560.7",)"
                 R"("low24h":"41147.5",)"
                 R"("sodUtc0":"41663.1",)"
                 R"("sodUtc8":"41984.2",)"
                 R"("ts":"1642643951284")"
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
    void operator()(
        Trace<json::BooksL2Tbt> const &, [[maybe_unused]] std::string_view const &inst_id, json::Action) override {
      FAIL();
    }
    void operator()(Trace<json::IndexTickers> const &event) override {
      ++count_;
      auto &[trace_info, trades] = event;
      auto &data = trades.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.inst_id == "BTC-USDT"sv);
      CHECK(d0.idx_px == 41756.7_a);
      CHECK(d0.open24h == 42427.3_a);
      CHECK(d0.high24h == 42560.7_a);
      CHECK(d0.low24h == 41147.5_a);
      CHECK(d0.sod_utc0 == 41663.1_a);
      CHECK(d0.sod_utc8 == 41984.2_a);
      CHECK(d0.ts == 1642643951284ms);
    }
    void operator()(Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(Trace<json::Login> const &) override { FAIL(); }
    void operator()(Trace<json::Account> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(Trace<json::Positions> const &) override { FAIL(); }
    void operator()(Trace<json::Orders> const &) override { FAIL(); }
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
