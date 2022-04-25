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

TEST_CASE("json_order_ack_parser_success", "[json_order_ack]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"abcABC123",)"
                 R"("ordId":"393513242072608785",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"",)"
                 R"("tag":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"1000001",)"
                 R"("msg":"",)"
                 R"("op":"order")"
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
    void operator()(Trace<json::FundingRate const> const &) override { FAIL(); }
    void operator()(Trace<json::Login const> const &) override { FAIL(); }
    void operator()(Trace<json::Account const> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition const> const &) override { FAIL(); }
    void operator()(Trace<json::Positions const> const &) override { FAIL(); }
    void operator()(Trace<json::Orders const> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck const> const &event) override {
      ++count_;
      auto &[trace_info, order_ack] = event;
      CHECK(order_ack.code == 0);
      auto &data = order_ack.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.cl_ord_id == "abcABC123"sv);
      CHECK(d0.ord_id == "393513242072608785"sv);
      CHECK(d0.s_code == 0);
      CHECK(d0.s_msg == ""sv);
      CHECK(d0.tag == ""sv);
      CHECK(order_ack.id == "1000001"sv);
      CHECK(order_ack.msg == ""sv);
      CHECK(order_ack.op == json::Operation::ORDER);
    }
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

TEST_CASE("json_order_ack_parser_failure", "[json_order_ack]") {
  auto message = R"({)"
                 R"("code":"1",)"
                 R"("data":[{)"
                 R"("clOrdId":"abcABC125",)"
                 R"("ordId":"",)"
                 R"("sCode":"51016",)"
                 R"("sMsg":"Duplicated client order ID",)"
                 R"("tag":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000001",)"
                 R"("msg":"",)"
                 R"("op":"order")"
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
    void operator()(Trace<json::FundingRate const> const &) override { FAIL(); }
    void operator()(Trace<json::Login const> const &) override { FAIL(); }
    void operator()(Trace<json::Account const> const &) override { FAIL(); }
    void operator()(Trace<json::BalanceAndPosition const> const &) override { FAIL(); }
    void operator()(Trace<json::Positions const> const &) override { FAIL(); }
    void operator()(Trace<json::Orders const> const &) override { FAIL(); }
    void operator()(Trace<json::OrderAck const> const &event) override {
      ++count_;
      auto &[trace_info, order_ack] = event;
      CHECK(order_ack.code == 1);
      auto &data = order_ack.data;
      REQUIRE(std::size(data) == 1);
      auto &d0 = data[0];
      CHECK(d0.cl_ord_id == "abcABC125"sv);
      CHECK(d0.ord_id == ""sv);
      CHECK(d0.s_code == 51016);
      CHECK(d0.s_msg == "Duplicated client order ID"sv);
      CHECK(d0.tag == ""sv);
      CHECK(order_ack.id == "2000001"sv);
      CHECK(order_ack.msg == ""sv);
      CHECK(order_ack.op == json::Operation::ORDER);
    }
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
