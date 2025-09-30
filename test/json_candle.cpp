/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_candles_parser", "[json_candles]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"candle1m",)"
                 R"("instId":"BTC-USDT")"
                 R"(},)"
                 R"("data":[)"
                 R"(["1757076060000","112600.1","112631.9","112477.1","112477.1","3.10944629","349947.335430728","349947.335430728","0"])"
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
    void operator()(Trace<json::Orders> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck> const &) override { FAIL(); }
    void operator()(Trace<json::Candle> const &event) override {
      ++count_;
      auto &[trace_info, candle] = event;
      auto &arg = candle.arg;
      CHECK(arg.channel == json::Channel::CANDLE1M);
      CHECK(arg.inst_id == "BTC-USDT"sv);
      REQUIRE(std::size(candle.data) == 1);
      auto &d = candle.data;
      CHECK(d[0].timestamp == 1757076060000ms);
    }

   private:
    size_t count_ = {};
  } handler;
  core::json::BufferStack buffer{8192, 1};
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info, false);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
