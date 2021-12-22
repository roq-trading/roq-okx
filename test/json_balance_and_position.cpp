/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include <gtest/gtest.h>

#include "roq/core/json/parser.h"

#include "roq/okex/json/parser.h"

using namespace roq;
using namespace roq::okex;

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
    void operator()(server::Trace<json::Login> const &) override { FAIL(); }
    void operator()(server::Trace<json::Account> const &) override { FAIL(); }
    void operator()(server::Trace<json::BalanceAndPosition> const &event) override {
      ++count_;
      auto &[trace_info, balance_and_position] = event;
      auto &bal_data = balance_and_position.bal_data;
      EXPECT_EQ(std::size(bal_data), 3);
      auto &bd0 = bal_data[0];
      EXPECT_DOUBLE_EQ(bd0.cash_bal, 0.0121475);
      EXPECT_EQ(bd0.ccy, "BTC"sv);
      EXPECT_EQ(bd0.u_time, 1640088676388ms);
      auto &bd1 = bal_data[1];
      EXPECT_DOUBLE_EQ(bd1.cash_bal, 0.00000635);
      EXPECT_EQ(bd1.ccy, "OMG"sv);
      EXPECT_EQ(bd1.u_time, 1624937815126ms);
      auto &bd2 = bal_data[2];
      EXPECT_DOUBLE_EQ(bd2.cash_bal, 0.00005916);
      EXPECT_EQ(bd2.ccy, "EOS"sv);
      EXPECT_EQ(bd2.u_time, 1624937815179ms);
      EXPECT_EQ(balance_and_position.event_type, "snapshot"sv);
      EXPECT_EQ(balance_and_position.p_time, 1640151123380ms);
      auto &pos_data = balance_and_position.pos_data;
      EXPECT_EQ(std::size(pos_data), 0);
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
