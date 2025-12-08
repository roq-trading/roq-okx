/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include <catch2/catch_all.hpp>

#include "roq/okx/json/parser.hpp"

namespace roq {
namespace okx {

template <typename T>
struct ParserTester final : public json::Parser::Handler {
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
    auto res = json::Parser::dispatch(handler, message, buffers, {}, false);
    CHECK(res == true);
    CHECK(handler.found_ == true);
  }

 protected:
  explicit ParserTester(callback_type const &callback) : callback_{callback} {}

  // events
  void operator()(Trace<json::Error> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Subscribe> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Unsubscribe> const &event) override { dispatch_helper(event); }
  // push
  // - public
  void operator()(Trace<json::Status> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Instruments> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::EstimatedPrice> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::PriceLimit> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::MarkPrice> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Tickers> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Trades> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::BboTbt> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::BooksL2Tbt> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::IndexTickers> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::FundingRate> const &event) override { dispatch_helper(event); }
  // - private
  // -- admin
  void operator()(Trace<json::ChannelConnCount> const &event) override { dispatch_helper(event); }
  // -- event
  void operator()(Trace<json::Login> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Account> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::BalanceAndPosition> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Positions> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::Orders> const &event) override { dispatch_helper(event); }
  // -- ack
  void operator()(Trace<json::Order> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::AmendOrder> const &event) override { dispatch_helper(event); }
  void operator()(Trace<json::CancelOrder> const &event) override { dispatch_helper(event); }
  // - business
  void operator()(Trace<json::Candle> const &event) override { dispatch_helper(event); }

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
