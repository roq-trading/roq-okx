/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "parser_tester.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

using value_type = protocol::json::Subscribe;

TEST_CASE("status", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"status")"
                 R"(},)"
                 R"("connId":"26c9b771")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::STATUS);
    CHECK(obj.conn_id == "26c9b771"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("instruments", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"instruments",)"
                 R"("instType":"SPOT")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::INSTRUMENTS);
    CHECK(obj.arg.inst_type == "SPOT"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("tickers", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"tickers",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::TICKERS);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("trades", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"trades",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::TRADES);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("bbo_tbt", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"bbo-tbt",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::BBO_TBT);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("books", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"books",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::BOOKS);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("index_tickers", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"index-tickers",)"
                 R"("instId":"BTC-USD")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::INDEX_TICKERS);
    CHECK(obj.arg.inst_id == "BTC-USD"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}

TEST_CASE("funding_rate", "[json_subscribe]") {
  auto message = R"({)"
                 R"("event":"subscribe",)"
                 R"("arg":{)"
                 R"("channel":"funding-rate",)"
                 R"("instId":"BTC-USD-SWAP")"
                 R"(},)"
                 R"("connId":"94d38802")"
                 R"(})";
  auto helper = [](value_type const &obj) {
    CHECK(obj.event == protocol::json::EventType::SUBSCRIBE);
    CHECK(obj.arg.channel == protocol::json::Channel::FUNDING_RATE);
    CHECK(obj.arg.inst_id == "BTC-USD-SWAP"sv);
    CHECK(obj.conn_id == "94d38802"sv);
  };
  ParserTester<value_type>::dispatch(helper, message, 8192, 1);
}
