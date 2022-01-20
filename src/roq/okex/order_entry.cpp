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
          .order_ack = create_metrics(name_, "order_ack"sv),
          .amend_order_ack = create_metrics(name_, "amend_order_ack"sv),
          .cancel_order_ack = create_metrics(name_, "cancel_order_ack"sv),
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
      .write(profile_.order_ack, metrics::PROFILE)
      .write(profile_.amend_order_ack, metrics::PROFILE)
      .write(profile_.cancel_order_ack, metrics::PROFILE)
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
      throw oms::NotSupported("not supported"sv);
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
      throw oms::NotSupported("not supported"sv);
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
        throw oms::NotSupported("not supported"sv);
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
          request_id,
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
          request_id,
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
    const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  auto &[message_info, modify_order] = event;
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id =
      has_external_order_id ? std::string_view{order.external_order_id} : previous_request_id;
  auto new_sz = std::isnan(modify_order.quantity) ? order.quantity : modify_order.quantity;
  auto new_px = std::isnan(modify_order.price) ? order.price : modify_order.price;
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("op":"amend-order",)"
      R"("args":[{{)"
      R"("{}":"{}",)"
      R"("instId":"{}",)"
      R"("reqId":"{}",)"
      R"("newSz":"{}",)"
      R"("newPx":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      ++request_id_,
      order_id_type,
      order_id,
      order.symbol,
      request_id,
      new_sz,
      new_px);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
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
      R"("id":"{}",)"
      R"("op":"cancel-order",)"
      R"("args":[{{)"
      R"("{}":"{}",)"
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
  // throw oms::NotSupported("not supported"sv);
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
      if (json::Parser::dispatch(*this, message, buffer, trace_info)) {
      } else {
        log::fatal(R"(message="{}")"sv, message);
      }
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

void OrderEntry::operator()(server::Trace<json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(server::Trace<json::FundingRate> const &) {
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
    // log::debug("event={{trace_info={}, account={}}}"sv, trace_info, account);
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
    // orders={code=0, msg="", arg={channel=ORDERS, inst_id="", inst_type="ANY",
    // uid="33594834598109184"}, data=[{acc_fill_sz=0, amend_result=0, avg_px=0, category=NORMAL,
    // ccy="", cl_ord_id="abcABC124", code=0, c_time=1640184258331ms, exec_type=UNDEFINED,
    // fee_ccy="BTC", fee=0, fill_fee_ccy="", fill_fee=0, fill_notional_usd="", fill_px=nan,
    // fill_sz=0, fill_time=0ms, inst_id="BTC-USD-220325", inst_type=FUTURES, lever=10, msg="",
    // notional_usd="100.0", ord_id="393896560769265665", ord_type=LIMIT, pnl=0, pos_side=LONG,
    // px=40007.4, rebate_ccy="BTC", rebate=0, reduce_only=false, req_id="", side=BUY,
    // sl_ord_px=nan, sl_trigger_px=nan, sl_trigger_px_type=UNDEFINED, source="", state=LIVE, sz=1,
    // tag="", td_mode=ISOLATED, tgt_ccy="", tp_ord_px=nan, tp_trigger_px=nan,
    // tp_trigger_px_type=UNDEFINED, trade_id="", u_time=1640184258331ms}]}}
    for (auto &item : orders.data) {
      auto side = json::map(item.side);
      auto order_status = json::map(item.state);
      oms::OrderUpdate order_update{
          .account = security_.get_account(),
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .side = side,
          .position_effect = {},
          .max_show_quantity = NaN,
          .order_type = {},
          .time_in_force = {},
          .execution_instruction = {},
          .order_template = {},
          .create_time_utc = {},
          .update_time_utc = utils::safe_cast(item.u_time),
          .external_account = {},
          .external_order_id = item.ord_id,
          .status = order_status,
          .quantity = item.sz,
          .price = item.px,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = item.acc_fill_sz,
          .average_traded_price = item.avg_px,
          .last_traded_quantity = item.fill_sz,
          .last_traded_price = item.fill_px,
          .last_liquidity = {},
      };
      if (shared_.update_order(
              item.cl_ord_id,
              stream_id_,
              trace_info,
              order_update,
              [&]([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("*** EXTERNAL ORDER ***"sv);
        log::warn("item={}"sv, item);
      }
    }
  });
}

void OrderEntry::operator()(server::Trace<json::OrderAck> const &event) {
  profile_.order_ack([&]() {
    auto &[trace_info, order_ack] = event;
    log::info<1>("event={{trace_info={}, order_ack={}}}"sv, trace_info, order_ack);
    log::debug("event={{trace_info={}, order_ack={}}}"sv, trace_info, order_ack);
    auto order_status = order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : order_ack.data) {
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = json::guess_error(item.s_code),
          .text = item.s_msg,
          .version = {},
          .request_id = item.cl_ord_id,
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
              })) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

void OrderEntry::operator()(server::Trace<json::AmendOrderAck> const &event) {
  profile_.amend_order_ack([&]() {
    auto &[trace_info, amend_order_ack] = event;
    log::info<1>("event={{trace_info={}, amend_order_ack={}}}"sv, trace_info, amend_order_ack);
    log::debug("event={{trace_info={}, amend_order_ack={}}}"sv, trace_info, amend_order_ack);
    auto order_status = amend_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : amend_order_ack.data) {
      oms::Response response{
          .type = RequestType::MODIFY_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = json::guess_error(item.s_code),
          .text = item.s_msg,
          .version = {},
          .request_id = item.req_id,
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
              })) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

void OrderEntry::operator()(server::Trace<json::CancelOrderAck> const &event) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, cancel_order_ack] = event;
    log::info<1>("event={{trace_info={}, cancel_order_ack={}}}"sv, trace_info, cancel_order_ack);
    log::debug("event={{trace_info={}, cancel_order_ack={}}}"sv, trace_info, cancel_order_ack);
    auto order_status = cancel_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : cancel_order_ack.data) {
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = json::guess_error(item.s_code),
          .text = item.s_msg,
          .version = {},
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {
              })) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

}  // namespace okex
}  // namespace roq
