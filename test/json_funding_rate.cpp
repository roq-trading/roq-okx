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
  return TraceInfo{
      .source_receive_time = {},
      .origin_create_time = {},
      .origin_create_time_utc = {},
  };
}
}  // namespace

TEST_CASE("json_funding_rate_parser", "[json_funding_rate]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"funding-rate",)"
                 R"("instId":"BTC-USD-SWAP")"
                 R"(},)"
                 R"("data":[{)"
                 R"("fundingRate":"-0.00006384",)"
                 R"("fundingTime":"1642665600000",)"
                 R"("instId":"BTC-USD-SWAP",)"
                 R"("instType":"SWAP",)"
                 R"("nextFundingRate":"-0.00005")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(Trace<json::Error const> const &) override { FAIL(); }
    void operator()(Trace<json::Subscribe const> const &) override { FAIL(); }
    void operator()(Trace<json::Unsubscribe const> const &) override { FAIL(); }
    void operator()(Trace<json::Status const> const &) override { FAIL(); }
    void operator()(Trace<json::Instruments const> const &) override { FAIL(); }
    void operator()(Trace<json::EstimatedPrice const> const &) override { FAIL(); }
    void operator()(Trace<json::PriceLimit const> const &) override { FAIL(); }
    void operator()(Trace<json::MarkPrice const> const &) override { FAIL(); }
    void operator()(Trace<json::Tickers const> const &) override { FAIL(); }
    void operator()(Trace<json::Trades const> const &) override { FAIL(); }
    void operator()(
        Trace<json::BooksL2Tbt const> const &,
        [[maybe_unused]] const std::string_view &inst_id,
        json::Action) override {
      FAIL();
    }
    void operator()(Trace<json::IndexTickers const> const &) override { FAIL(); }
    void operator()(Trace<json::FundingRate const> const &event) override {
      ++count_;
      auto &[trace_info, trades] = event;
      auto &data = trades.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.funding_rate == -0.00006384_a);
      CHECK(d0.funding_time == 1642665600000ms);
      CHECK(d0.inst_id == "BTC-USD-SWAP"sv);
      CHECK(d0.inst_type == json::InstrumentType::SWAP);
      CHECK(d0.next_funding_rate == -0.00005_a);
    }
    void operator()(Trace<json::Login const> const &) override { FAIL(); }
    void operator()(Trace<json::Account const> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition const> const &) override { FAIL(); }
    void operator()(Trace<json::Positions const> const &) override { FAIL(); }
    void operator()(Trace<json::Orders const> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck const> const &) override { FAIL(); }
    void operator()(Trace<json::AmendOrderAck const> const &) override { FAIL(); }
    void operator()(Trace<json::CancelOrderAck const> const &) override { FAIL(); }

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
