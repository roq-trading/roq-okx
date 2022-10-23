/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/order_entry.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "roq/mask.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/oms/exceptions.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/core/json/buffer.hpp"

#include "roq/web/socket/client_factory.hpp"

#include "roq/server.hpp"

#include "roq/okx/flags.hpp"

#include "roq/okx/json/order_type.hpp"
#include "roq/okx/json/position_side.hpp"
#include "roq/okx/json/trade_mode.hpp"
#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "ex"sv;

auto const SUPPORTS = Mask{
    SupportType::CREATE_ORDER,
    SupportType::MODIFY_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::ws_private_uri();
  web::socket::Client::Config config{
      .always_reconnect = true,
      .connection_timeout = server::Flags::net_connection_timeout(),
      .disconnect_on_idle_timeout = {},
      .validate_certificate = server::Flags::net_tls_validate_certificate(),
      .uris = {&uri, 1},
      .query = {},
      .ping_frequency = Flags::ws_ping_freq(),
      .read_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
  };
  return web::socket::ClientFactory::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto const &group, auto const &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

OrderEntry::OrderEntry(
    Handler &handler, io::Context &context, uint16_t stream_id, Security &security, Shared &shared, Request &request)
    : handler_(handler), stream_id_(stream_id), name_(create_name(stream_id_, security.get_account())),
      connection_(create_connection(*this, context)), decode_buffer_(Flags::decode_buffer_size()),
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
      security_(security), shared_(shared), request_(request),
      download_({}, [this](auto state) { return download(state); }), trade_mode_(flags::Flags::trade_mode()) {
}

bool OrderEntry::ready() const {
  return (*connection_).ready();
}

void OrderEntry::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntry::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntry::operator()(Event<Timer> const &event) {
  (*connection_).refresh(event.value.now);
  check_response_orders();
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
std::pair<json::OrderType, bool> compute_order_attributes(
    auto const &order_type, auto const &time_in_force, auto const &execution_instructions) {
  bool reduce_only = false;
  json::OrderType order_type_ = {};
  if (!std::empty(execution_instructions)) {
    if (execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE))
      order_type_ = json::OrderType::POST_ONLY;
    if (execution_instructions.has(ExecutionInstruction::DO_NOT_INCREASE))
      reduce_only = true;
    // throw oms::NotSupported{"not supported"sv};
  }
  switch (time_in_force) {
    using enum TimeInForce;
    case GTC:
      break;
    case FOK:
      if (order_type_ != json::OrderType{})
        order_type_ = json::OrderType::FOK;
      break;
    case IOC:
      if (order_type_ != json::OrderType{})
        order_type_ = json::OrderType::IOC;
      break;
    default:
      throw oms::NotSupported{"not supported"sv};
  }
  if (order_type_ == json::OrderType{}) {
    switch (order_type) {
      using enum OrderType;
      case MARKET:
        order_type_ = json::OrderType::MARKET;
        break;
      case LIMIT:
        order_type_ = json::OrderType::LIMIT;
        break;
      default:
        throw oms::NotSupported{"not supported"sv};
    }
  }
  return {order_type_, reduce_only};
}
}  // namespace

uint16_t OrderEntry::operator()(
    Event<CreateOrder> const &event, oms::Order const &, std::string_view const &request_id) {
  auto &[message_info, create_order] = event;
  json::PositionSide position_side = json::PositionSide::NET;  // XXX should be configurable
  auto side = json::map(create_order.side);
  auto [order_type, reduce_only] = compute_order_attributes(
      create_order.order_type, create_order.time_in_force, create_order.execution_instructions);
  switch (order_type) {
    using enum json::OrderType::type_t;
    case MARKET: {
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("op":"batch-orders",)"
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
          trade_mode_.as_raw_text(),
          position_side.as_raw_text(),
          create_order.symbol,
          side.as_raw_text(),
          order_type.as_raw_text(),
          reduce_only,
          create_order.quantity);
      log::debug("message={}"sv, message);
      (*connection_).send_text(message);
      break;
    }
    default: {
      auto message = fmt::format(
          R"({{)"
          R"("id":"{}",)"
          R"("op":"batch-orders",)"
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
          trade_mode_.as_raw_text(),
          position_side.as_raw_text(),
          create_order.symbol,
          side.as_raw_text(),
          order_type.as_raw_text(),
          reduce_only,
          create_order.quantity,
          create_order.price);
      log::debug("message={}"sv, message);
      (*connection_).send_text(message);
    }
  }
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    Event<ModifyOrder> const &event,
    oms::Order const &order,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, modify_order] = event;
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id = has_external_order_id ? std::string_view{order.external_order_id} : previous_request_id;
  auto new_sz = std::isnan(modify_order.quantity) ? order.quantity : modify_order.quantity;
  auto new_px = std::isnan(modify_order.price) ? order.price : modify_order.price;
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("op":"batch-amend-orders",)"
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
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    Event<CancelOrder> const &,
    oms::Order const &order,
    [[maybe_unused]] std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto has_external_order_id = !std::empty(order.external_order_id);
  auto order_id_type = has_external_order_id ? "ordId"sv : "clOrdId"sv;
  auto order_id = has_external_order_id ? std::string_view{order.external_order_id} : previous_request_id;
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("op":"batch-cancel-orders",)"
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
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t OrderEntry::operator()(Event<CancelAllOrders> const &, [[maybe_unused]] std::string_view const &request_id) {
  std::vector<std::pair<std::string_view, std::string_view>> symbol_and_external_order_ids;
  if (shared_.dispatcher_.get_all_orders(
          [&](auto &order) {
            log::debug("order={}"sv, order);
            symbol_and_external_order_ids.emplace_back(order.symbol, order.external_order_id);
          },
          security_.get_account())) {
  } else {
    log::info<1>("No orders"sv);
  }
  cancel_all_orders(symbol_and_external_order_ids);
  return stream_id_;
}

void OrderEntry::operator()(web::socket::Client::Connected const &) {
}

void OrderEntry::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void OrderEntry::operator()(web::socket::Client::Ready const &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void OrderEntry::operator()(web::socket::Client::Close const &) {
}

void OrderEntry::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = security_.get_account(),
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void OrderEntry::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t OrderEntry::download(OrderEntryState state) {
  switch (state) {
    using enum OrderEntryState;
    case UNDEFINED:
      assert(false);
      break;
    case LOGIN:
      login();
      return 1;
    case SUBSCRIBE:
      subscribe();
      return 1;
    case ORDERS:
      request_orders();
      return 1;
    case DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

void OrderEntry::login() {
  auto now = clock::get_realtime<std::chrono::seconds>();
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
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void OrderEntry::subscribe() {
  subscribe("account"sv);
  subscribe("balance_and_position"sv);
  subscribe("positions"sv, "instType"sv, "ANY"sv);
  subscribe("orders"sv, "instType"sv, "ANY"sv);
}

void OrderEntry::subscribe(std::string_view const &channel) {
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
  (*connection_).send_text(message);
}

void OrderEntry::subscribe(
    std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
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
  (*connection_).send_text(message);
}

void OrderEntry::parse(std::string_view const &message) {
  profile_.parse([&]() {
    try {
      // log::debug(R"(message="{}")"sv, message);
      TraceInfo trace_info;
      core::json::Buffer buffer{decode_buffer_};
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

void OrderEntry::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::fatal("event={{error={}, trace_info={}}}"sv, error, trace_info);
  });
}

void OrderEntry::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{subscribe={}, trace_info={}}}"sv, subscribe, trace_info);
    log::debug("event={{subscribe={}, trace_info={}}}"sv, subscribe, trace_info);
    if (subscribe.channel == json::Channel::ORDERS)
      download_.check(OrderEntryState::SUBSCRIBE);
  });
}

void OrderEntry::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
    log::debug("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
  });
}

void OrderEntry::operator()(Trace<json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::FundingRate> const &) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::BboTbt> const &, [[maybe_unused]] std::string_view const &inst_id) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(
    Trace<json::BooksL2Tbt> const &, [[maybe_unused]] std::string_view const &inst_id, json::Action) {
  log::fatal("Unexpected"sv);
}

void OrderEntry::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("event={{login={}, trace_info={}}}"sv, login, trace_info);
    log::debug("login={}"sv, login);
    auto state = OrderEntryState::LOGIN;
    download_.check_relaxed(state);
  });
}

void OrderEntry::operator()(Trace<json::Account> const &event) {
  profile_.account([&]() {
    auto &[trace_info, account] = event;
    log::info<1>("event={{account={}, trace_info={}}}"sv, account, trace_info);
    // log::debug("account={}"sv, account);
    for (auto &item : account.details) {
      FundsUpdate funds_update{
          .stream_id = stream_id_,
          .account = security_.get_account(),
          .currency = item.ccy,
          .balance = item.cash_bal,
          .hold = item.frozen_bal,
          .external_account = {},
      };
      create_trace_and_dispatch(handler_, trace_info, funds_update, true);
    }
  });
}

void OrderEntry::operator()(Trace<json::BalanceAndPosition> const &event) {
  profile_.balance_and_position([&]() {
    auto &[trace_info, balance_and_position] = event;
    log::info<1>("event={{balance_and_position={}, trace_info={}}}"sv, balance_and_position, trace_info);
    // log::debug("balance_and_position={}"sv, balance_and_position);
  });
}

void OrderEntry::operator()(Trace<json::Positions> const &event) {
  profile_.positions([&]() {
    auto &[trace_info, positions] = event;
    log::info<1>("event={{positions={}, trace_info={}}}"sv, positions, trace_info);
    // log::debug("positions={}"sv, positions);
    for (auto &item : positions.data) {
      auto long_quantity = std::max(0.0, item.pos);
      auto short_quantity = std::max(0.0, -item.pos);
      PositionUpdate position_update{
          .stream_id = stream_id_,
          .account = security_.get_account(),
          .exchange = Flags::exchange(),
          .symbol = item.inst_id,
          .external_account = {},
          .long_quantity = long_quantity,
          .short_quantity = short_quantity,
          .long_quantity_begin = NaN,
          .short_quantity_begin = NaN,
      };
      create_trace_and_dispatch(handler_, trace_info, position_update, true);
    }
  });
}

void OrderEntry::operator()(Trace<json::Orders> const &event) {
  profile_.orders([&]() {
    auto &[trace_info, orders] = event;
    log::info<1>("event={{orders={}, trace_info={}}}"sv, orders, trace_info);
    log::debug("orders={}"sv, orders);
    for (auto &item : orders.data) {
      if (item.amend_result < 0)
        log::warn<1>("*** AMEND HAS FAILED ***"sv);
      if (item.code != 0)
        log::warn<1>(R"(*** ERROR CODE={}, MSG="{}" ***)"sv, item.code, item.msg);
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
          .execution_instructions = {},
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
          .update_type = {},
      };
      if (shared_.update_order(
              item.cl_ord_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("*** EXTERNAL ORDER ***"sv);
        log::warn("item={}"sv, item);
      }
    }
  });
}

void OrderEntry::operator()(Trace<json::OrderAck> const &event) {
  profile_.order_ack([&]() {
    auto &[trace_info, order_ack] = event;
    log::info<1>("event={{order_ack={}, trace_info={}}}"sv, order_ack, trace_info);
    log::debug("order_ack={}"sv, order_ack);
    auto order_status = order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : order_ack.data) {
      auto error = json::guess_error(item.s_code);
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = error,
          .text = item.s_msg,
          .version = {},
          .request_id = item.cl_ord_id,
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

void OrderEntry::operator()(Trace<json::AmendOrderAck> const &event) {
  profile_.amend_order_ack([&]() {
    auto &[trace_info, amend_order_ack] = event;
    log::info<1>("event={{amend_order_ack={}, trace_info={}}}"sv, amend_order_ack, trace_info);
    log::debug("amend_order_ack={}"sv, amend_order_ack);
    auto order_status = amend_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : amend_order_ack.data) {
      auto error = json::guess_error(item.s_code);
      oms::Response response{
          .type = RequestType::MODIFY_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = error,
          .text = item.s_msg,
          .version = {},
          .request_id = item.req_id,
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

void OrderEntry::operator()(Trace<json::CancelOrderAck> const &event) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, cancel_order_ack] = event;
    log::info<1>("event={{cancel_order_ack={}, trace_info={}}}"sv, cancel_order_ack, trace_info);
    log::debug("cancel_order_ack={}"sv, cancel_order_ack);
    auto order_status = cancel_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : cancel_order_ack.data) {
      auto error = json::guess_error(item.s_code);
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = order_status,
          .error = error,
          .text = item.s_msg,
          .version = {},
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(item.cl_ord_id, stream_id_, trace_info, response, []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(R"(Did not find order: cl_ord_id="{}")"sv, item.cl_ord_id);
      }
    }
  });
}

void OrderEntry::cancel_all_orders(
    std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_ids) {
  for (auto &[symbol, external_order_id] : symbol_and_external_order_ids) {
    auto message = fmt::format(
        R"({{)"
        R"("id":"{}",)"
        R"("op":"batch-cancel-orders",)"
        R"("args":[{{)"
        R"("instId":"{}",)"
        R"("ordId":"{}")"
        R"(}})"
        R"(])"
        R"(}})"sv,
        ++request_id_,
        symbol,
        external_order_id);
    log::debug("message={}"sv, message);
    (*connection_).send_text(message);
  }
}

void OrderEntry::request_orders() {
  log::info("Requesting order download..."sv);
  request_.request_orders = clock::get_system();
}

void OrderEntry::check_response_orders() {
  if (download_.state() != OrderEntryState::ORDERS)
    return;
  if (request_.request_orders < request_.respond_orders) {
    log::info("Order download has completed!"sv);
    download_.check(OrderEntryState::ORDERS);
  }
}

}  // namespace okx
}  // namespace roq
