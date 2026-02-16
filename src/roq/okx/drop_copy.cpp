/* Copyright (c) 2017-2026, Hans Erik Thrane */

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

#include "roq/core/json/buffer.hpp"

#include "roq/server/oms/exceptions.hpp"

#include "roq/okx/json/map.hpp"

#include "roq/okx/json/encoder.hpp"
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

size_t const MAX_DECODE_BUFFER_DEPTH = 2;
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

uint16_t DropCopy::operator()(
    Event<CreateOrder> const &event, server::oms::Order const &order, server::oms::RefData const &, std::string_view const &request_id) {
  auto &[message_info, create_order] = event;
  auto message = json::Encoder::batch_orders(encode_buffer_, create_order, order, request_id, request_id_, trade_mode_, shared_.settings.test_margin_currency);
  // log::warn(R"(DEBUG message="{}")"sv, message);
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t DropCopy::operator()(
    Event<ModifyOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, modify_order] = event;
  auto message = json::Encoder::batch_amend_orders(encode_buffer_, modify_order, order, request_id, previous_request_id, request_id_);
  // log::warn(R"(DEBUG message="{}")"sv, message);
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t DropCopy::operator()(
    Event<CancelOrder> const &event,
    server::oms::Order const &order,
    server::oms::RefData const &,
    std::string_view const &request_id,
    std::string_view const &previous_request_id) {
  auto &[message_info, cancel_order] = event;
  auto message = json::Encoder::batch_cancel_orders(encode_buffer_, cancel_order, order, request_id, previous_request_id, request_id_);
  // log::warn(R"(DEBUG message="{}")"sv, message);
  (*connection_).send_text(message);
  return stream_id_;
}

uint16_t DropCopy::operator()(Event<CancelAllOrders> const &event, [[maybe_unused]] std::string_view const &request_id) {
  auto &[message_info, cancel_all_orders] = event;
  // XXX FIXME TODO what about orders where we haven't received external_order_id ???
  std::vector<std::pair<std::string_view, std::string_view>> symbol_and_external_order_id;
  if (shared_.dispatcher_.get_all_orders(
          [&](auto &order) {
            if (!std::empty(order.external_order_id)) {
              symbol_and_external_order_id.emplace_back(order.symbol, order.external_order_id);
            }
          },
          cancel_all_orders)) {
  } else {
    log::info<1>("No orders"sv);
  }
  if (!std::empty(symbol_and_external_order_id)) {
    auto message = json::Encoder::batch_cancel_orders(encode_buffer_, cancel_all_orders, request_id, request_id_, symbol_and_external_order_id);
    // log::warn(R"(DEBUG message="{}")"sv, message);
    (*connection_).send_text(message);
  }
  // XXX FIXME TODO CancelAllOrdersAck
  return stream_id_;
}

// web::socket::Client::Handler

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

// json::Parser::Handler

void DropCopy::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::fatal("error={}"sv, error);
  });
}

void DropCopy::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("subscribe={}"sv, subscribe);
    if (subscribe.arg.channel == json::Channel::ORDERS) {
      download_.check(DropCopyState::SUBSCRIBE);
    }
  });
}

void DropCopy::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("unsubscribe={}"sv, unsubscribe);
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

void DropCopy::operator()(Trace<json::BboTbt> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::BooksL2Tbt> const &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(Trace<json::ChannelConnCount> const &event) {
  profile_.channel_conn_count([&]() {
    auto &[trace_info, channel_conn_count] = event;
    log::info<1>("channel_conn_count={}"sv, channel_conn_count);
  });
}

void DropCopy::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("login={}"sv, login);
    auto state = DropCopyState::LOGIN;
    download_.check_relaxed(state);
  });
}

void DropCopy::operator()(Trace<json::Account> const &event) {
  profile_.account([&]() {
    auto &[trace_info, account] = event;
    log::info<1>("account={}"sv, account);
    for (auto &item : account.data) {
      for (auto &item_2 : item.details) {
        auto funds_update = FundsUpdate{
            .stream_id = stream_id_,
            .account = account_.name,
            .currency = item_2.ccy,
            .margin_mode = {},
            .balance = item_2.cash_bal,
            .hold = item_2.frozen_bal,
            .borrowed = NaN,
            .external_account = {},
            .update_type = {},
            .exchange_time_utc = {},
            .sending_time_utc = {},
        };
        create_trace_and_dispatch(handler_, trace_info, funds_update, true);
      }
    }
  });
}

void DropCopy::operator()(Trace<json::BalanceAndPosition> const &event) {
  profile_.balance_and_position([&]() {
    auto &[trace_info, balance_and_position] = event;
    log::info<1>("balance_and_position={}"sv, balance_and_position);
  });
}

void DropCopy::operator()(Trace<json::Positions> const &event) {
  profile_.positions([&]() {
    auto &[trace_info, positions] = event;
    log::info<1>("positions={}"sv, positions);
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
    log::info<1>("orders={}"sv, orders);
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
          .error = {},
          .text = {},
          .quantity = item.sz,
          .price = item.px,
          .stop_price = NaN,
          .leverage = NaN,
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

void DropCopy::operator()(Trace<json::Order> const &event) {
  profile_.order_ack([&]() {
    auto &[trace_info, order_ack] = event;
    log::info<1>("order_ack={}"sv, order_ack);
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
          .external_order_id = item.ord_id,
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

void DropCopy::operator()(Trace<json::AmendOrder> const &event) {
  profile_.amend_order_ack([&]() {
    auto &[trace_info, amend_order_ack] = event;
    log::info<1>("amend_order_ack={}"sv, amend_order_ack);
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
          .external_order_id = item.ord_id,
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

void DropCopy::operator()(Trace<json::CancelOrder> const &event) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, cancel_order_ack] = event;
    log::info<1>("cancel_order_ack={}"sv, cancel_order_ack);
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
          .external_order_id = item.ord_id,
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
