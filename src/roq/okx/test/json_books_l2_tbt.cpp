/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_books_l2_tbt_parser", "[json_books_l2_tbt]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"books-l2-tbt",)"
                 R"("instId":"BTC-USD-220325"},)"
                 R"("action":"snapshot",)"
                 R"("data":[{)"
                 R"("asks":[)"
                 R"(["50441.1","24","0","1"],)"
                 R"(["50449.1","152","0","3"])"
                 R"(],)"
                 R"("bids":[)"
                 R"(["50441","199","0","3"],)"
                 R"(["50438.2","49","0","1"])"
                 R"(],)"
                 R"("ts":"1640158295741",)"
                 R"("checksum":-386306878)"
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
        Trace<json::BooksL2Tbt> const &event, std::string_view const &inst_id, json::Action action) override {
      ++count_;
      auto &[trace_info, books_l2_tbt] = event;
      CHECK(inst_id == "BTC-USD-220325"sv);
      CHECK(action == json::Action::SNAPSHOT);
      auto &asks = books_l2_tbt.asks;
      REQUIRE(std::size(asks) == 2);
      // a0
      auto &a0 = asks[0];
      CHECK(a0.price == 50441.1_a);
      CHECK(a0.size == 24.0_a);
      CHECK(a0.liquidated_orders == 0);
      CHECK(a0.orders == 1);
      // a1
      auto &bids = books_l2_tbt.bids;
      REQUIRE(std::size(bids) == 2);
      // b0
      auto &b0 = bids[0];
      CHECK(b0.price == 50441_a);
      CHECK(b0.size == 199.0_a);
      CHECK(b0.liquidated_orders == 0);
      CHECK(b0.orders == 3);
    }
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

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}

TEST_CASE("json_books_l2_tbt_parser_books5", "[json_books_l2_tbt]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"books5",)"
                 R"("instId":"PERP-USDT")"
                 R"(},)"
                 R"("data":[{)"
                 R"("asks":[)"
                 R"(["0.773","2.160647","0","2"],)"
                 R"(["0.774","1348.469769","0","5"],)"
                 R"(["0.775","1563.669577","0","3"],)"
                 R"(["0.776","1276.742269","0","3"],)"
                 R"(["0.777","192","0","1"])"
                 R"(],)"
                 R"("bids":[)"
                 R"(["0.771","1111.147468","0","5"],)"
                 R"(["0.77","727.91547","0","3"],)"
                 R"(["0.769","5428.837019","0","4"],)"
                 R"(["0.768","2720.056497","0","3"],)"
                 R"(["0.767","2006.682024","0","2"])"
                 R"(],)"
                 R"("instId":"PERP-USDT",)"
                 R"("ts":"1656326389967")"
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
        Trace<json::BooksL2Tbt> const &event, std::string_view const &inst_id, json::Action action) override {
      ++count_;
      auto &[trace_info, books_l2_tbt] = event;
      CHECK(inst_id == "PERP-USDT"sv);
      CHECK(action == json::Action::SNAPSHOT);
      auto &asks = books_l2_tbt.asks;
      REQUIRE(std::size(asks) == 5);
      // a0
      auto &a0 = asks[0];
      CHECK(a0.price == 0.773_a);
      CHECK(a0.size == 2.160647_a);
      CHECK(a0.liquidated_orders == 0);
      CHECK(a0.orders == 2);
      // a1...
      auto &bids = books_l2_tbt.bids;
      REQUIRE(std::size(bids) == 5);
      // b0
      auto &b0 = bids[0];
      CHECK(b0.price == 0.771_a);
      CHECK(b0.size == 1111.147468_a);
      CHECK(b0.liquidated_orders == 0);
      CHECK(b0.orders == 5);
      // b1...
    }
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

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
