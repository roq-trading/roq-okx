/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

using namespace roq;
using namespace roq::okx;

using namespace std::literals;
using namespace std::chrono_literals;

using namespace Catch::literals;

TEST_CASE("json_status_parser", "[json_status]") {
  auto message = R"({)"
                 R"("arg":{)"
                 R"("channel":"status")"
                 R"(},)"
                 R"("data":[{)"
                 R"("begin":"1639645200000",)"
                 R"("end":"1639647600000",)"
                 R"("href":"",)"
                 R"("scheDesc":"",)"
                 R"("serviceType":"0",)"
                 R"("state":"completed",)"
                 R"("system":"unified",)"
                 R"("title":"Unified Account WebSocket system upgrade",)"
                 R"("ts":"1639647646363")"
                 R"(})"
                 R"(])"
                 R"(})";
  struct MyHandler final : public json::Parser::Handler {
    auto get_count() const { return count_; }

   protected:
    void operator()(Trace<json::Error> const &) override { FAIL(); }
    void operator()(Trace<json::Subscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Unsubscribe> const &) override { FAIL(); }
    void operator()(Trace<json::Status> const &event) override {
      ++count_;
      auto &[trace_info, status] = event;
      CHECK(status.begin == 1639645200000ms);
      CHECK(status.end == 1639647600000ms);
      CHECK(status.href == ""sv);
      CHECK(status.sche_desc == ""sv);
      CHECK(status.service_type == 0);
      CHECK(status.state == json::State::COMPLETED);
      CHECK(status.system == "unified"sv);
      CHECK(status.title == "Unified Account WebSocket system upgrade"sv);
      CHECK(status.ts == 1639647646363ms);
    }
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

   private:
    size_t count_ = {};
  } handler;
  std::vector<std::byte> buffer(8192);
  TraceInfo trace_info;
  auto res = json::Parser::dispatch(handler, message, buffer, trace_info);
  CHECK(res == true);
  CHECK(handler.get_count() == 1);
}
