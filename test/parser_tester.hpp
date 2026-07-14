/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/protocol/json/parser.hpp"

namespace roq {
namespace okx {

template <typename T>
struct ParserTester final : public protocol::json::Parser::Handler {
  using value_type = std::remove_cvref_t<T>;
  using callback_type = std::function<void(value_type const &)>;

  static void dispatch(callback_type const &callback, std::string_view const &message, size_t buffer_size, size_t max_depth) {
    core::json::BufferStack buffers{buffer_size, max_depth};
    // simple
    // XXX FIXME TODO catch2 block ???
    T obj{message, buffers};
    callback(obj);
    // parser
    // XXX FIXME TODO catch2 block ???
    ParserTester handler{callback};
    auto res = protocol::json::Parser::dispatch(handler, message, buffers, {}, false);
    CHECK(res == true);
    CHECK(handler.found_ == true);
  }

 protected:
  explicit ParserTester(callback_type const &callback) : callback_{callback} {}

  // events
  void operator()(Trace<protocol::json::Error> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Subscribe> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Unsubscribe> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Notice> const &event) override { dispatch_helper(event); }
  // push
  // - public
  void operator()(Trace<protocol::json::Status> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Instruments> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::EstimatedPrice> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::PriceLimit> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::MarkPrice> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Tickers> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Trades> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::BboTbt> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::BooksL2Tbt> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::IndexTickers> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::FundingRate> const &event) override { dispatch_helper(event); }
  // - private
  // -- admin
  void operator()(Trace<protocol::json::ChannelConnCount> const &event) override { dispatch_helper(event); }
  // -- event
  void operator()(Trace<protocol::json::Login> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Account> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::BalanceAndPosition> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Positions> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::Orders> const &event) override { dispatch_helper(event); }
  // -- ack
  void operator()(Trace<protocol::json::Order> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::AmendOrder> const &event) override { dispatch_helper(event); }
  void operator()(Trace<protocol::json::CancelOrder> const &event) override { dispatch_helper(event); }
  // - business
  void operator()(Trace<protocol::json::Candle> const &event) override { dispatch_helper(event); }

  template <typename U>
  void dispatch_helper(Trace<U> const &event) {
    if constexpr (std::is_invocable_v<callback_type, U>) {
      found_ = true;
      callback_(event);
    } else {
      FAIL();
    }
  }

 private:
  callback_type const callback_;
  bool found_ = false;
};

}  // namespace okx
}  // namespace roq
