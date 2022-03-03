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

TEST_CASE("json_tickers_parser", "json_tickers") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"tickers",)"
                 R"("instId":"BTC-USD-220325"},)"
                 R"("data":[{)"
                 R"("instType":"FUTURES",)"
                 R"("instId":"BTC-USD-220325",)"
                 R"("last":"50326.8",)"
                 R"("lastSz":"1",)"
                 R"("askPx":"50330.5",)"
                 R"("askSz":"19",)"
                 R"("bidPx":"50330.4",)"
                 R"("bidSz":"51",)"
                 R"("open24h":"49905.2",)"
                 R"("high24h":"50900",)"
                 R"("low24h":"49474",)"
                 R"("sodUtc0":"50107.5",)"
                 R"("sodUtc8":"49646.5",)"
                 R"("volCcy24h":"7544.4991",)"
                 R"("vol24h":"3778623",)"
                 R"("ts":"1640156513987")"
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
    void operator()(server::Trace<json::Tickers> const &event) override {
      ++count_;
      auto &[trace_info, tickers] = event;
      auto &data = tickers.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.inst_type == json::InstrumentType::FUTURES);
      CHECK(d0.inst_id == "BTC-USD-220325"sv);
      CHECK(d0.last == 50326.8_a);
      CHECK(d0.last_sz == 1.0_a);
      CHECK(d0.ask_px == 50330.5_a);
      CHECK(d0.ask_sz == 19.0_a);
      CHECK(d0.bid_px == 50330.4_a);
      CHECK(d0.bid_sz == 51.0_a);
      CHECK(d0.open24h == 49905.2_a);
      CHECK(d0.high24h == 50900.0_a);
      CHECK(d0.low24h == 49474.0_a);
      CHECK(d0.sod_utc0 == 50107.5_a);
      CHECK(d0.sod_utc8 == 49646.5_a);
      CHECK(d0.vol_ccy24h == 7544.4991_a);
      CHECK(d0.vol24h == 3778623.0_a);
      CHECK(d0.ts == 1640156513987ms);
    }
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
