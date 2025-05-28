/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_amend_order_ack_parser_success", "[json_amend_order_ack]") {
  auto message = R"({)"
                 R"("code":"0",)"
                 R"("data":[{)"
                 R"("clOrdId":"CMAAF2IDAAAQAAFQDIHPD4Y3",)"
                 R"("ordId":"393936310213439488",)"
                 R"("reqId":"",)"
                 R"("sCode":"0",)"
                 R"("sMsg":"")"
                 R"(})"
                 R"(],)"
                 R"("id":"2000002",)"
                 R"("msg":"",)"
                 R"("op":"amend-order")"
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
    void operator()(Trace<json::AmendOrderAck> const &event) override {
      ++count_;
      auto &[trace_info, amend_order_ack] = event;
      CHECK(amend_order_ack.code == 0);
      auto &data = amend_order_ack.data;
      REQUIRE(std::size(data) == 1);
      auto &data_0 = data[0];
      CHECK(data_0.cl_ord_id == "CMAAF2IDAAAQAAFQDIHPD4Y3"sv);
      CHECK(data_0.ord_id == "393936310213439488"sv);
      CHECK(std::empty(data_0.req_id));
      CHECK(data_0.s_code == 0);
      CHECK(std::empty(data_0.s_msg));
      CHECK(std::empty(data_0.tag));
      CHECK(amend_order_ack.id == "2000002"sv);
      CHECK(std::empty(amend_order_ack.msg));
      CHECK(amend_order_ack.op == json::Operation::AMEND_ORDER);
    }
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
