/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/order_entry.h"

#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/oms/exceptions.h"

#include "roq/core/metrics/factory.h"

#include "roq/core/json/buffer.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/order_type.h"
#include "roq/okex/json/position_side.h"
#include "roq/okex/json/trade_mode.h"
#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static const auto NAME = "ex"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::CREATE_ORDER,
    SupportType::MODIFY_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

OrderEntry::OrderEntry(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
          *this,
          context,
          Flags::ws_private_uri(),
          {},
          Flags::ws_ping_freq(),
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      decode_buffer_(Flags::decode_buffer_size()),
      request_id_(static_cast<uint64_t>(stream_id_) * 1000000),  // scale (debugging)
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .error = create_metrics(name_, "error"sv),
          .subscribe = create_metrics(name_, "subscribe"sv),
          .unsubscribe = create_metrics(name_, "unsubscribe"sv),
          .login = create_metrics(name_, "login"sv),
          .account = create_metrics(name_, "account"sv),
          .balance_and_position = create_metrics(name_, "balance_and_position"sv),
          .positions = create_metrics(name_, "positions"sv),
          .orders = create_metrics(name_, "orders"sv),
          .create_order = create_metrics(name_, "create_order"sv),
          .modify_order = create_metrics(name_, "modify_order"sv),
          .cancel_order = create_metrics(name_, "cancel_order"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      security_(security), shared_(shared),
      download_({}, [this](auto state) { return download(state); }) {
}

bool OrderEntry::ready() const {
  return connection_.ready();
}

void OrderEntry::operator()(const Event<Start> &) {
  connection_.start();
}

void OrderEntry::operator()(const Event<Stop> &) {
  connection_.stop();
}

void OrderEntry::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

void OrderEntry::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.subscribe, metrics::PROFILE)
      .write(profile_.unsubscribe, metrics::PROFILE)
      .write(profile_.login, metrics::PROFILE)
      .write(profile_.account, metrics::PROFILE)
      .write(profile_.balance_and_position, metrics::PROFILE)
      .write(profile_.positions, metrics::PROFILE)
      .write(profile_.orders, metrics::PROFILE)
      .write(profile_.create_order, metrics::PROFILE)
      .write(profile_.modify_order, metrics::PROFILE)
      .write(profile_.cancel_order, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

namespace {
static std::pair<json::OrderType, bool> compute_order_attributes(
    OrderType order_type, TimeInForce time_in_force, ExecutionInstruction execution_instruction) {
  bool reduce_only = false;
  json::OrderType order_type_;
  switch (execution_instruction) {
    case ExecutionInstruction::UNDEFINED:
      break;
    case ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE:
      order_type_ = json::OrderType::POST_ONLY;
      break;
    case ExecutionInstruction::DO_NOT_INCREASE:
      reduce_only = true;
      break;
    default:
      throw oms::NotSupportedException();
  }
  switch (time_in_force) {
    case TimeInForce::GTC:
      break;
    case TimeInForce::FOK:
      if (order_type_ != json::OrderType{})
        order_type_ = json::OrderType::FOK;
      break;
    case TimeInForce::IOC:
      if (order_type_ != json::OrderType{})
        order_type_ = json::OrderType::IOC;
      break;
    default:
      throw oms::NotSupportedException();
  }
  if (order_type_ == json::OrderType{}) {
    switch (order_type) {
      case OrderType::MARKET:
        order_type_ = json::OrderType::MARKET;
        break;
      case OrderType::LIMIT:
        order_type_ = json::OrderType::LIMIT;
        break;
      default:
        throw oms::NotSupportedException();
    }
  }
  return {order_type_, reduce_only};
}
}  // namespace

uint16_t OrderEntry::operator()(
    const Event<CreateOrder> &event, const oms::Order &, const std::string_view &request_id) {
  auto &[message_info, create_order] = event;
  json::TradeMode trade_mode = json::TradeMode::ISOLATED;  // XXX maybe different for spot/futures
  // cash didn't work for future
  json::PositionSide position_side =
      json::PositionSide::NET;               // XXX maybe infer from side + pos eff
  position_side = json::PositionSide::LONG;  // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  auto side = json::map(create_order.side);
  auto [order_type, reduce_only] = compute_order_attributes(
      create_order.order_type, create_order.time_in_force, create_order.execution_instruction);
  auto XXX = "abcABC125"sv;  // XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  switch (order_type) {
    case json::OrderType::MARKET: {
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("op":"order",)"
          R"("args":[{{)"
          R"("clOrdId":"{}",)"
          R"("tdMode":"{}",)"
          R"("posSide":"{}",)"
          R"("instId":"{}",)"
          R"("side":"{}",)"
          R"("ordType":"{}",)"
          R"("reduceOnly":{},)"
          R"("sz":"{}")"
          R"(}})"
          R"(])"
          R"(}})"sv,
          ++request_id_,
          XXX,  // request_id,
          trade_mode.as_raw_text(),
          position_side.as_raw_text(),
          create_order.symbol,
          side.as_raw_text(),
          order_type.as_raw_text(),
          reduce_only,
          create_order.quantity);
      log::debug("message={}"sv, message);
      connection_.send_text(message);
      break;
    }
    default: {
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("op":"order",)"
          R"("args":[{{)"
          R"("clOrdId":"{}",)"
          R"("tdMode":"{}",)"
          R"("posSide":"{}",)"
          R"("instId":"{}",)"
          R"("side":"{}",)"
          R"("ordType":"{}",)"
          R"("reduceOnly":{},)"
          R"("sz":"{}",)"
          R"("px":"{}")"
          R"(}})"
          R"(])"
          R"(}})"sv,
          ++request_id_,
          XXX,  // request_id,
          trade_mode.as_raw_text(),
          position_side.as_raw_text(),
          create_order.symbol,
          side.as_raw_text(),
          order_type.as_raw_text(),
          reduce_only,
          create_order.quantity,
          create_order.price);
      log::debug("message={}"sv, message);
      connection_.send_text(message);
    }
  }
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<ModifyOrder> &event,
    const oms::Order &order,
    [[maybe_unused]] const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  auto &[message_info, modify_order] = event;
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id =
      has_external_order_id ? std::string_view{order.external_order_id} : previous_request_id;
  auto new_sz = std::isnan(modify_order.quantity) ? order.quantity : modify_order.quantity;
  auto new_px = std::isnan(modify_order.price) ? order.price : modify_order.price;
  auto message = fmt::format(
      R"({)"
      R"("id":"{}",)"
      R"("op":"amend-order",)"
      R"("args":[{)"
      R"("{}":"{}",)"
      R"("instId":"{}",)"
      R"("newSz":"{}",)"
      R"("newPx":"{}")"
      R"(})"
      R"(])"
      R"(})"sv,
      ++request_id_,
      order_id_type,
      order_id,
      order.symbol,
      new_sz,
      new_px);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<CancelOrder> &,
    const oms::Order &order,
    [[maybe_unused]] const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id =
      has_external_order_id ? std::string_view{order.external_order_id} : previous_request_id;
  auto message = fmt::format(
      R"({{)"
      R"("id":"",)"
      R"("op":"cancel-order",)"
      R"("args":[{{)"
      R"("{}":"{},")"
      R"("instId":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      ++request_id_,
      order_id_type,
      order_id,
      order.symbol);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<CancelAllOrders> &, [[maybe_unused]] const std::string_view &request_id) {
  // throw oms::NotSupportedException();
  return stream_id_;
}

void OrderEntry::operator()(const core::web::ClientSocket::Connected &) {
}

void OrderEntry::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void OrderEntry::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void OrderEntry::operator()(const core::web::ClientSocket::Close &) {
}

void OrderEntry::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry::operator()(const core::web::ClientSocket::Text &text) {
  parse(text.payload);
}

void OrderEntry::operator()(const core::web::ClientSocket::Binary &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::WEB_SOCKET,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t OrderEntry::download(OrderEntryState state) {
  switch (state) {
    case OrderEntryState::UNDEFINED:
      assert(false);
      break;
    case OrderEntryState::LOGIN:
      login();
      return 1;
    case OrderEntryState::SUBSCRIBE:
      subscribe();
      return {};
    case OrderEntryState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

void OrderEntry::login() {
  std::chrono::seconds now = utils::safe_cast(core::get_realtime_clock());
  auto timestamp = fmt::format("{}"sv, now.count());
  auto sign = security_.create_sign(timestamp);
  auto message = fmt::format(
      R"({{)"
      R"("op":"login",)"
      R"("args":[{{)"
      R"("apiKey":"{}",)"
      R"("passphrase":"{}",)"
      R"("timestamp":"{}",)"
      R"("sign":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      security_.get_key(),
      security_.get_passphrase(),
      timestamp,
      sign);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void OrderEntry::subscribe() {
  subscribe("account"sv);
  subscribe("balance_and_position"sv);
  subscribe("positions"sv, "instType"sv, "ANY"sv);
  subscribe("orders"sv, "instType"sv, "ANY"sv);
}

void OrderEntry::subscribe(const std::string_view &channel) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void OrderEntry::subscribe(
    const std::string_view &channel,
    const std::string_view &selector,
    const std::string_view &value) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}",)"
      R"("{}":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel,
      selector,
      value);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void OrderEntry::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      log::debug(R"(message="{}")"sv, message);
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
      json::Parser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void OrderEntry::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::fatal("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void OrderEntry::operator()(server::Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
    log::debug("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
  });
}

void OrderEntry::operator()(server::Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
    log::debug("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
  });
}

void OrderEntry::operator()(server::Trace<json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(
    server::Trace<json::BooksL2Tbt> const &,
    [[maybe_unused]] const std::string_view &inst_id,
    json::Action) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("event={{trace_info={}, login={}}}"sv, trace_info, login);
    log::debug("event={{trace_info={}, login={}}}"sv, trace_info, login);
    auto state = OrderEntryState::LOGIN;
    download_.check_relaxed(state);
  });
}

void OrderEntry::operator()(server::Trace<json::Account> const &event) {
  profile_.account([&]() {
    auto &[trace_info, account] = event;
    log::info<1>("event={{trace_info={}, account={}}}"sv, trace_info, account);
    log::debug("event={{trace_info={}, account={}}}"sv, trace_info, account);
    // XXX HANS
  });
}

void OrderEntry::operator()(server::Trace<json::BalanceAndPosition> const &event) {
  profile_.balance_and_position([&]() {
    auto &[trace_info, balance_and_position] = event;
    log::info<1>(
        "event={{trace_info={}, balance_and_position={}}}"sv, trace_info, balance_and_position);
    log::debug(
        "event={{trace_info={}, balance_and_position={}}}"sv, trace_info, balance_and_position);
    // XXX HANS
  });
}

void OrderEntry::operator()(server::Trace<json::Positions> const &event) {
  profile_.positions([&]() {
    auto &[trace_info, positions] = event;
    log::info<1>("event={{trace_info={}, positions={}}}"sv, trace_info, positions);
    log::debug("event={{trace_info={}, positions={}}}"sv, trace_info, positions);
    // XXX HANS
  });
}

void OrderEntry::operator()(server::Trace<json::Orders> const &event) {
  profile_.orders([&]() {
    auto &[trace_info, orders] = event;
    log::info<1>("event={{trace_info={}, orders={}}}"sv, trace_info, orders);
    log::debug("event={{trace_info={}, orders={}}}"sv, trace_info, orders);
    // XXX HANS
  });
}

}  // namespace okex
}  // namespace roq
