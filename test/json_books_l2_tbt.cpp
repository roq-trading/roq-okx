/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = json::BooksL2Tbt;

// note! truncated
TEST_CASE("books_snapshot", "[json_books_l2_tbt]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"books",)"
                 R"("instId":"NEAR-USDT")"
                 R"(},)"
                 R"("action":"snapshot",)"
                 R"("data":[{)"
                 R"("asks":[)"
                 R"(["1.783","411.15719291","0","3"],)"
                 R"(["1.784","5842.06737585","0","13"],)"
                 R"(["2.194","1.02075576","0","1"])"
                 R"(],)"
                 R"("bids":[)"
                 R"(["1.782","2173.46443964","0","8"],)"
                 R"(["1.781","14620.34390321","0","30"],)"
                 R"(["1.32","541.81794985","0","4"])"
                 R"(],)"
                 R"("ts":"1765201257005",)"
                 R"("checksum":-1197821412,)"
                 R"("seqId":9690930615,)"
                 R"("prevSeqId":-1)"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::BOOKS);
    CHECK(obj.arg.inst_id == "NEAR-USDT"sv);
    CHECK(obj.action == json::Action::SNAPSHOT);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 3);
}

TEST_CASE("books_update", "[json_books_l2_tbt]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"books",)"
                 R"("instId":"NEAR-USDT")"
                 R"(},)"
                 R"("action":"update",)"
                 R"("data":[{)"
                 R"("asks":[],)"
                 R"("bids":[)"
                 R"(["1.781","14694.07509503","0","31"],)"
                 R"(["1.78","12102.991384","0","32"])"
                 R"(],)"
                 R"("ts":"1765201257205",)"
                 R"("checksum":1923732318,)"
                 R"("seqId":9690930617,)"
                 R"("prevSeqId":9690930615)"
                 R"(})"
                 R"(])"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::BOOKS);
    CHECK(obj.arg.inst_id == "NEAR-USDT"sv);
    CHECK(obj.action == json::Action::UPDATE);
    REQUIRE(std::size(obj.data) == 1);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 3);
}

TEST_CASE("books5", "[json_books_l2_tbt]") {
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
  auto helper = [](value_type const &obj) {
    CHECK(obj.arg.channel == json::Channel::BOOKS5);
    CHECK(obj.arg.inst_id == "PERP-USDT"sv);
    CHECK(obj.action == json::Action::UNDEFINED_INTERNAL);
    REQUIRE(std::size(obj.data) == 1);
    auto &asks = obj.data[0].asks;
    REQUIRE(std::size(asks) == 5);
    // ask_0
    auto &ask_0 = asks[0];
    CHECK(ask_0.price == 0.773_a);
    CHECK(ask_0.size == 2.160647_a);
    CHECK(ask_0.liquidated_orders == 0);
    CHECK(ask_0.orders == 2);
    // a1...
    auto &bids = obj.data[0].bids;
    REQUIRE(std::size(bids) == 5);
    // bid_0
    auto &bid_0 = bids[0];
    CHECK(bid_0.price == 0.771_a);
    CHECK(bid_0.size == 1111.147468_a);
    CHECK(bid_0.liquidated_orders == 0);
    CHECK(bid_0.orders == 5);
    // b1...
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 3);
}
