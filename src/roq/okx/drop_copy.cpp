/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/okx/drop_copy.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "roq/mask.hpp"

#include "roq/utils/common.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/core/json/buffer.hpp"

#include "roq/server.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/okx/json/map.hpp"
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

uint64_t const REQUEST_ID = 1'000'000;

size_t const MAX_DECODE_BUFFER_DEPTH = 1;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id, auto const &account) {
  return fmt::format("{}:{}:{}"sv, stream_id, NAME, account);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.ws.private_uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = settings.misc.test_local_interface,
      .uris = {&uri, 1},
      .host = settings.ws.private_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = {},
      .always_reconnect = true,
      // proxy
      .proxy = {},
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = {},
      .ping_frequency = settings.ws.ping_freq,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::socket::Client::create(handler, context, config, []() { return std::string(); });
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};

std::pair<json::OrderType, bool> compute_order_attributes(auto order_type, auto time_in_force, auto execution_instructions) {
  auto log_no_mapping_exists = [&]() {
    log::error("No mapping exists for order_type={}, time_in_force={}, execution_instructions={}"sv, order_type, time_in_force, execution_instructions);
  };
  bool reduce_only = false;
  json::OrderType order_type_ = {};
  if (!std::empty(execution_instructions)) {
    if (execution_instructions.has(ExecutionInstruction::PARTICIPATE_DO_NOT_INITIATE)) {
      order_type_ = json::OrderType::POST_ONLY;
    }
    if (execution_instructions.has(ExecutionInstruction::DO_NOT_INCREASE)) {
      reduce_only = true;
    }
    // throw server::oms::NotSupported{"not supported"sv};
  }
  switch (time_in_force) {
    using enum TimeInForce;
    case UNDEFINED:
    case GTC:
      break;
    case FOK:
      if (order_type_ != json::OrderType{}) {
        order_type_ = json::OrderType::FOK;
      }
      break;
    case IOC:
      if (order_type_ != json::OrderType{}) {
        order_type_ = json::OrderType::IOC;
      }
      break;
    default:
      log_no_mapping_exists();
      throw server::oms::NotSupported{"not supported"sv};
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
        log_no_mapping_exists();
        throw server::oms::NotSupported{"not supported"sv};
    }
  }
  return {order_type_, reduce_only};
}
}  // namespace

// === IMPLEMENTATION ===

DropCopy::DropCopy(Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_, account.name)}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH}, request_id_{static_cast<uint64_t>(stream_id_) * REQUEST_ID},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .error = create_metrics(shared.settings, name_, "error"sv),
          .subscribe = create_metrics(shared.settings, name_, "subscribe"sv),
          .unsubscribe = create_metrics(shared.settings, name_, "unsubscribe"sv),
          .channel_conn_count = create_metrics(shared.settings, name_, "channel_conn_count"sv),
          .login = create_metrics(shared.settings, name_, "login"sv),
          .account = create_metrics(shared.settings, name_, "account"sv),
          .balance_and_position = create_metrics(shared.settings, name_, "balance_and_position"sv),
          .positions = create_metrics(shared.settings, name_, "positions"sv),
          .orders = create_metrics(shared.settings, name_, "orders"sv),
          .create_order = create_metrics(shared.settings, name_, "create_order"sv),
          .modify_order = create_metrics(shared.settings, name_, "modify_order"sv),
          .cancel_order = create_metrics(shared.settings, name_, "cancel_order"sv),
          .order_ack = create_metrics(shared.settings, name_, "order_ack"sv),
          .amend_order_ack = create_metrics(shared.settings, name_, "amend_order_ack"sv),
          .cancel_order_ack = create_metrics(shared.settings, name_, "cancel_order_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      account_{account}, shared_{shared}, request_{request}, download_{{}, [this](auto state) { return download(state); }},
      trade_mode_{shared.settings.trade_mode} {
}

bool DropCopy::ready() const {
  return (*connection_).ready();
}

void DropCopy::operator()(Event<Start> const &) {
  (*connection_).start();
}

void DropCopy::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void DropCopy::operator()(Event<Timer> const &event) {
  (*connection_).refresh(event.value.now);
  check_response_balance();
  check_response_positions();
  check_response_orders();
}

void DropCopy::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.subscribe, metrics::Type::PROFILE)
      .write(profile_.unsubscribe, metrics::Type::PROFILE)
      .write(profile_.channel_conn_count, metrics::Type::PROFILE)
      .write(profile_.login, metrics::Type::PROFILE)
      .write(profile_.account, metrics::Type::PROFILE)
      .write(profile_.balance_and_position, metrics::Type::PROFILE)
      .write(profile_.positions, metrics::Type::PROFILE)
      .write(profile_.orders, metrics::Type::PROFILE)
      .write(profile_.create_order, metrics::Type::PROFILE)
      .write(profile_.modify_order, metrics::Type::PROFILE)
      .write(profile_.cancel_order, metrics::Type::PROFILE)
      .write(profile_.order_ack, metrics::Type::PROFILE)
      .write(profile_.amend_order_ack, metrics::Type::PROFILE)
      .write(profile_.cancel_order_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

uint16_t DropCopy::operator()(Event<CreateOrder> const &event, server::oms::Order const &order, std::string_view const &request_id) {
  auto &[message_info, create_order] = event;
  json::PositionSide position_side = json::PositionSide::NET;  // XXX should be configurable
  auto side = map(create_order.side).template get<json::Side>();
  auto [order_type, reduce_only] = compute_order_attributes(create_order.order_type, create_order.time_in_force, create_order.execution_instructions);
  auto trade_mode = [&]() -> json::TradeMode {
    switch (create_order.margin_mode) {
      using enum MarginMode;
      case UNDEFINED:
        break;
      case ISOLATED:
        return json::TradeMode::type_t::ISOLATED;
      case CROSS:
        return json::TradeMode::type_t::CROSS;
      case PORTFOLIO:
        throw server::oms::Rejected{Origin::GATEWAY, Error::INVALID_REQUEST_ARGS, "margin_mode"sv};
    }
    return trade_mode_;
  }();
  std::string extras;
  if (trade_mode == json::TradeMode::type_t::CROSS && !std::empty(shared_.settings.test_margin_currency)) {
    extras = fmt::format(R"(,"ccy":"{}")"sv, shared_.settings.test_margin_currency);
  }
  switch (order_type) {
    using enum json::OrderType::type_t;
    case MARKET: {
      std::string message;
      if (order.security_type == SecurityType::SPOT) {
        message = fmt::format(
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
            R"("tgtCcy":"base_ccy",)"  // note!
            R"("sz":"{}")"
            R"({})"  // extras
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
            extras);
      } else {
        message = fmt::format(
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
            R"({})"  // extras
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
            extras);
      }
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
          R"({})"  // extras
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
          create_order.price,
          extras);
      (*connection_).send_text(message);
    }
  }
  return stream_id_;
}

uint16_t DropCopy::operator()(
    Event<ModifyOrder> const &event, server::oms::Order const &order, std::string_view const &request_id, std::string_view const &previous_request_id) {
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
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t DropCopy::operator()(
    Event<CancelOrder> const &,
    server::oms::Order const &order,
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
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t DropCopy::operator()(Event<CancelAllOrders> const &event, [[maybe_unused]] std::string_view const &request_id) {
  auto &cancel_all_orders_2 = event.value;
  std::vector<std::pair<std::string_view, std::string_view>> symbol_and_external_order_ids;
  if (shared_.dispatcher_.get_all_orders(
          [&](auto &order) { symbol_and_external_order_ids.emplace_back(order.symbol, order.external_order_id); }, cancel_all_orders_2)) {
  } else {
    log::info<1>("No orders"sv);
  }
  cancel_all_orders(symbol_and_external_order_ids);
  // XXX FIXME TODO CancelAllOrdersAck
  return stream_id_;
}

void DropCopy::operator()(web::socket::Client::Connected const &) {
}

void DropCopy::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
}

void DropCopy::operator()(web::socket::Client::Ready const &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void DropCopy::operator()(web::socket::Client::Close const &) {
}

void DropCopy::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = account_.name,
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void DropCopy::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void DropCopy::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = account_.name,
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::WS,
        .encoding = {Encoding::JSON},
        .priority = Priority::PRIMARY,
        .connection_status = status_,
        .interface = (*connection_).get_interface(),
        .authority = (*connection_).get_current_authority(),
        .path = (*connection_).get_current_path(),
        .proxy = (*connection_).get_proxy(),
    };
    log::info("stream_status={}"sv, stream_status);
    create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t DropCopy::download(DropCopyState state) {
  switch (state) {
    using enum DropCopyState;
    case UNDEFINED:
      assert(false);
      break;
    case LOGIN:
      login();
      return 1;
    case SUBSCRIBE:
      subscribe();
      return 1;
    case BALANCE:
      request_balance();
      return 1;
    case POSITIONS:
      request_positions();
      return 1;
    case ORDERS:
      request_orders();
      return 1;
    case DONE:
      (*this)(ConnectionStatus::READY);
      return 0;
  }
  assert(false);
  return 0;
}

void DropCopy::login() {
  auto now = clock::get_realtime<std::chrono::seconds>();
  auto timestamp = fmt::format("{}"sv, now.count());
  auto sign = account_.create_sign(timestamp);
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
      account_.get_key(),
      account_.get_passphrase(),
      timestamp,
      sign);
  (*connection_).send_text(message);
  (*this)(ConnectionStatus::LOGIN_SENT);
}

void DropCopy::subscribe() {
  subscribe("account"sv);
  subscribe("balance_and_position"sv);
  subscribe("positions"sv, "instType"sv, "ANY"sv);
  subscribe("orders"sv, "instType"sv, "ANY"sv);
}

void DropCopy::subscribe(std::string_view const &channel) {
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[{{)"
      R"("channel":"{}")"
      R"(}})"
      R"(])"
      R"(}})"sv,
      channel);
  (*connection_).send_text(message);
}

void DropCopy::subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
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
  (*connection_).send_text(message);
}

void DropCopy::parse(std::string_view const &message) {
  profile_.parse([&]() {
    auto log_message = [&]() { log::warn(R"(*** PLEASE REPORT *** message="{}")"sv, message); };
    try {
      TraceInfo trace_info;
      if (!json::Parser::dispatch(*this, message, decode_buffer_, trace_info, shared_.settings.experimental.allow_unknown_event_types)) {
        log_message();
      }
    } catch (...) {
      log_message();
      utils::exceptions::Unhandled::terminate();
    }
  });
}

void DropCopy::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::fatal("event={{error={}, trace_info={}}}"sv, error, trace_info);
  });
}

void DropCopy::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{subscribe={}, trace_info={}}}"sv, subscribe, trace_info);
    if (subscribe.channel == json::Channel::ORDERS) {
      download_.check(DropCopyState::SUBSCRIBE);
    }
  });
}

void DropCopy::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{unsubscribe={}, trace_info={}}}"sv, unsubscribe, trace_info);
  });
}

void DropCopy::operator()(Trace<json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::EstimatedPrice> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::PriceLimit> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::MarkPrice> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::Tickers> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::Trades> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::IndexTickers> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::FundingRate> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::BboTbt> const &, [[maybe_unused]] std::string_view const &inst_id) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::BooksL2Tbt> const &, [[maybe_unused]] std::string_view const &inst_id, json::Action) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::ChannelConnCount> const &event) {
  profile_.channel_conn_count([&]() {
    auto &[trace_info, channel_conn_count] = event;
    log::info<1>("event={{channel_conn_count={}, trace_info={}}}"sv, channel_conn_count, trace_info);
  });
}

void DropCopy::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("event={{login={}, trace_info={}}}"sv, login, trace_info);
    auto state = DropCopyState::LOGIN;
    download_.check_relaxed(state);
  });
}

void DropCopy::operator()(Trace<json::Account> const &event) {
  profile_.account([&]() {
    auto &[trace_info, account] = event;
    log::info<1>("event={{account={}, trace_info={}}}"sv, account, trace_info);
    for (auto &item : account.details) {
      auto funds_update = FundsUpdate{
          .stream_id = stream_id_,
          .account = account_.name,
          .currency = item.ccy,
          .margin_mode = {},
          .balance = item.cash_bal,
          .hold = item.frozen_bal,
          .borrowed = NaN,
          .external_account = {},
          .update_type = {},
          .exchange_time_utc = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, funds_update, true);
    }
  });
}

void DropCopy::operator()(Trace<json::BalanceAndPosition> const &event) {
  profile_.balance_and_position([&]() {
    auto &[trace_info, balance_and_position] = event;
    log::info<1>("event={{balance_and_position={}, trace_info={}}}"sv, balance_and_position, trace_info);
  });
}

void DropCopy::operator()(Trace<json::Positions> const &event) {
  profile_.positions([&]() {
    auto &[trace_info, positions] = event;
    log::info<1>("event={{positions={}, trace_info={}}}"sv, positions, trace_info);
    for (auto &item : positions.data) {
      auto long_quantity = std::max(0.0, item.pos);
      auto short_quantity = std::max(0.0, -item.pos);
      auto position_update = PositionUpdate{
          .stream_id = stream_id_,
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .margin_mode = {},
          .external_account = {},
          .long_quantity = long_quantity,
          .short_quantity = short_quantity,
          .update_type = {},
          .exchange_time_utc = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, position_update, true);
    }
  });
}

void DropCopy::operator()(Trace<json::Orders> const &event) {
  profile_.orders([&]() {
    auto &[trace_info, orders] = event;
    log::info<1>("event={{orders={}, trace_info={}}}"sv, orders, trace_info);
    for (auto &item : orders.data) {
      log::info<2>("item={}"sv, item);
      if (item.amend_result < 0) {
        log::warn<1>("*** AMEND HAS FAILED ***"sv);
      }
      if (item.code != 0) {
        log::warn<1>(R"(*** ERROR CODE={}, MSG="{}" ***)"sv, item.code, item.msg);
      }
      auto order_update = server::oms::OrderUpdate{
          .account = account_.name,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .side = map(item.side),
          .position_effect = {},
          .margin_mode = {},
          .max_show_quantity = NaN,
          .order_type = {},
          .time_in_force = {},
          .execution_instructions = {},
          .create_time_utc = utils::safe_cast(item.c_time),
          .update_time_utc = utils::safe_cast(item.u_time),
          .external_account = {},
          .external_order_id = item.ord_id,
          .client_order_id = {},
          .order_status = map(item.state),
          .quantity = item.sz,
          .price = item.px,
          .stop_price = NaN,
          .remaining_quantity = NaN,
          .traded_quantity = item.acc_fill_sz,
          .average_traded_price = item.avg_px,
          .last_traded_quantity = item.fill_sz,
          .last_traded_price = item.fill_px,
          .last_liquidity = {},
          .routing_id = {},
          .max_request_version = {},
          .max_response_version = {},
          .max_accepted_version = {},
          .update_type = UpdateType::INCREMENTAL,
          .sending_time_utc = utils::safe_cast(item.u_time),
      };
      if (shared_.update_order(item.cl_ord_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
        if (item.exec_type != json::OrderFlowType{}) {
          auto side = map(item.side).template get<Side>();
          auto ref_data = shared_.get_ref_data(shared_.settings.exchange, item.inst_id);
          auto profit_loss_amount = utils::compute_profit_loss_amount(side, item.fill_sz, item.fill_px, ref_data.multiplier);
          auto fill = Fill{
              .exchange_time_utc = utils::safe_cast(item.c_time),
              .external_trade_id = {},
              .quantity = item.fill_sz,
              .price = item.fill_px,
              .liquidity = map(item.exec_type),
              .commission_amount = item.fill_fee,
              .commission_currency = item.fill_fee_ccy,
              .base_amount = NaN,
              .quote_amount = NaN,
              .profit_loss_amount = profit_loss_amount,
          };
          fmt::format_to(std::back_inserter(fill.external_trade_id), "{}"sv, item.trade_id);
          auto trade_update = TradeUpdate{
              .stream_id = stream_id_,
              .account = account_.name,
              .order_id = {},
              .exchange = shared_.settings.exchange,
              .symbol = item.inst_id,
              .side = map(item.side),
              .position_effect = {},
              .margin_mode = {},
              .quantity_type = {},
              .create_time_utc = utils::safe_cast(item.c_time),
              .update_time_utc = utils::safe_cast(item.u_time),
              .external_account = {},
              .external_order_id = item.ord_id,
              .client_order_id = {},
              .fills = {&fill, 1},
              .routing_id = {},
              .update_type = UpdateType::INCREMENTAL,
              .sending_time_utc = utils::safe_cast(item.u_time),
              .user = {},
              .strategy_id = {},
          };
          create_trace_and_dispatch(handler_, trace_info, trade_update, true, SOURCE_NONE, item.cl_ord_id);
        }
      } else {
        log::warn("*** EXTERNAL ORDER ***"sv);
        log::warn("item={}"sv, item);
      }
    }
  });
}

void DropCopy::operator()(Trace<json::OrderAck> const &event) {
  profile_.order_ack([&]() {
    auto &[trace_info, order_ack] = event;
    log::info<1>("event={{order_ack={}, trace_info={}}}"sv, order_ack, trace_info);
    auto order_status = order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : order_ack.data) {
      auto error = json::guess_error(item.s_code);
      auto response = server::oms::Response{
          .request_type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = order_status,
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

void DropCopy::operator()(Trace<json::AmendOrderAck> const &event) {
  profile_.amend_order_ack([&]() {
    auto &[trace_info, amend_order_ack] = event;
    log::info<1>("event={{amend_order_ack={}, trace_info={}}}"sv, amend_order_ack, trace_info);
    auto order_status = amend_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : amend_order_ack.data) {
      auto error = json::guess_error(item.s_code);
      auto response = server::oms::Response{
          .request_type = RequestType::MODIFY_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = order_status,
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

void DropCopy::operator()(Trace<json::CancelOrderAck> const &event) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, cancel_order_ack] = event;
    log::info<1>("event={{cancel_order_ack={}, trace_info={}}}"sv, cancel_order_ack, trace_info);
    auto order_status = cancel_order_ack.code ? RequestStatus::REJECTED : RequestStatus::ACCEPTED;
    for (auto &item : cancel_order_ack.data) {
      auto error = json::guess_error(item.s_code);
      auto response = server::oms::Response{
          .request_type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .request_status = order_status,
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

void DropCopy::operator()(Trace<json::Candle> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::cancel_all_orders(std::span<std::pair<std::string_view, std::string_view>> const &symbol_and_external_order_ids) {
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
    (*connection_).send_text(message);
  }
}

// request

void DropCopy::request_balance() {
  log::info("Requesting balance download..."sv);
  request_.request_balance = clock::get_system();
}

void DropCopy::request_positions() {
  log::info("Requesting positions download..."sv);
  request_.request_positions = clock::get_system();
}

void DropCopy::request_orders() {
  log::info("Requesting order download..."sv);
  request_.request_orders = clock::get_system();
}

// response

void DropCopy::check_response_balance() {
  if (download_.state() != DropCopyState::BALANCE) {
    return;
  }
  if (request_.request_balance < request_.respond_balance) {
    log::info("Balance download has completed!"sv);
    download_.check(DropCopyState::BALANCE);
  }
}

void DropCopy::check_response_positions() {
  if (download_.state() != DropCopyState::POSITIONS) {
    return;
  }
  if (request_.request_positions < request_.respond_positions) {
    log::info("Positions download has completed!"sv);
    download_.check(DropCopyState::POSITIONS);
  }
}

void DropCopy::check_response_orders() {
  if (download_.state() != DropCopyState::ORDERS) {
    return;
  }
  if (request_.request_orders < request_.respond_orders) {
    log::info("Order download has completed!"sv);
    download_.check(DropCopyState::ORDERS);
  }
}

}  // namespace okx
}  // namespace roq
