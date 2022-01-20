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

TEST(json_cancel_order_ack, parser_success) {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"3MAAF2IDAAAQAAGSKMZCT5A3",)"
                 R"("ordId":"393940260828377089",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000003",)"
                 R"("msg":"",)"
                 R"("op":"cancel-order")"
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
    void operator()(server::Trace<json::IndexTickers> const &) override { FAIL(); }
    void operator()(server::Trace<json::FundingRate> const &) override { FAIL(); }
    void operator()(server::Trace<json::Login> const &) override { FAIL(); }
    void operator()(server::Trace<json::Account> const &) override { FAIL(); }
    void operator()(server::Trace<json::BalanceAndPosition> const &) override { FAIL(); }
    void operator()(server::Trace<json::Positions> const &) override { FAIL(); }
    void operator()(server::Trace<json::Orders> const &) override { FAIL(); }
    void operator()(server::Trace<json::OrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::AmendOrderAck> const &) override { FAIL(); }
    void operator()(server::Trace<json::CancelOrderAck> const &event) override {
      ++count_;
      auto &[trace_info, cancel_order_ack] = event;
      EXPECT_EQ(cancel_order_ack.code, 0);
      auto &data = cancel_order_ack.data;
      ASSERT_EQ(std::size(data), 1);
      auto &d0 = data[0];
      EXPECT_EQ(d0.cl_ord_id, "3MAAF2IDAAAQAAGSKMZCT5A3"sv);
      EXPECT_EQ(d0.ord_id, "393940260828377089"sv);
      EXPECT_EQ(d0.s_code, 0);
      EXPECT_EQ(d0.s_msg, ""sv);
      EXPECT_EQ(d0.tag, ""sv);
      EXPECT_EQ(cancel_order_ack.id, "2000003"sv);
      EXPECT_EQ(cancel_order_ack.msg, ""sv);
      EXPECT_EQ(cancel_order_ack.op, json::Operation::CANCEL_ORDER);
    }

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

// {"event":"error","msg":"channel:estimated-price,instType:FUTURES doesn't exist","code":"60018"}"
