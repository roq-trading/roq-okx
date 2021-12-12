/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/order_entry.h"

#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/number.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/metrics/factory.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static const auto NAME = "om"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::CREATE_ORDER,
    SupportType::CANCEL_ORDER,
    SupportType::ORDER_ACK,
    SupportType::FUNDS,
};

static const auto ALLOW_PIPELINING = true;

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

static auto create_query_string(
    std::chrono::nanoseconds now,
    std::chrono::nanoseconds period,
    size_t current_page,
    size_t page_size,
    bool active) {
  if (period.count()) {
    std::chrono::milliseconds start_at = utils::safe_cast(now - period);
    if (active)
      return fmt::format(
          "?currentPage={}&pageSize={}&startAt={}&status=active"sv,
          current_page,
          page_size,
          start_at.count());
    return fmt::format(
        "?currentPage={}&pageSize={}&startAt={}"sv, current_page, page_size, start_at.count());
  }
  if (active)
    return fmt::format("?currentPage={}&pageSize={}&status=active"sv, current_page, page_size);
  return fmt::format("?currentPage={}&pageSize={}"sv, current_page, page_size);
}

static auto compute_order_status(double size, double deal_size, bool is_active) {
  if (is_active)
    return OrderStatus::WORKING;
  return utils::compare(size, deal_size) == 0 ? OrderStatus::COMPLETED : OrderStatus::CANCELED;
}
}  // namespace

OrderEntry::OrderEntry(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared)
    : handler_(handler), stream_id_(stream_id),
      name_(fmt::format("{}:{}:{}"sv, stream_id_, NAME, security.get_account())),
      connection_(
          *this,
          context,
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          core::URI(Flags::rest_uri()),
          ROQ_PACKAGE_NAME,
          core::http::Connection::KEEP_ALIVE,
          ALLOW_PIPELINING,
          Flags::rest_request_timeout(),
          Flags::rest_ping_freq(),
          Flags::rest_ping_path()),
      decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .private_token = create_metrics(name_, "private_token"sv),
          .private_token_ack = create_metrics(name_, "private_token_ack"sv),
          .accounts = create_metrics(name_, "accounts"sv),
          .accounts_ack = create_metrics(name_, "accounts_ack"sv),
          .orders = create_metrics(name_, "orders"sv),
          .orders_ack = create_metrics(name_, "orders_ack"sv),
          .fills = create_metrics(name_, "fills"sv),
          .fills_ack = create_metrics(name_, "fills_ack"sv),
          .create_order = create_metrics(name_, "create_order"sv),
          .create_order_ack = create_metrics(name_, "create_order_ack"sv),
          .cancel_order = create_metrics(name_, "cancel_order"sv),
          .cancel_order_ack = create_metrics(name_, "cancel_order_ack"sv),
          .cancel_all_orders = create_metrics(name_, "cancel_all_orders"sv),
          .cancel_all_orders_ack = create_metrics(name_, "cancel_all_orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void OrderEntry::operator()(const Event<Start> &) {
  connection_.start();
}

void OrderEntry::operator()(const Event<Stop> &) {
  connection_.stop();
}

void OrderEntry::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
}

void OrderEntry::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.private_token, metrics::PROFILE)
      .write(profile_.private_token_ack, metrics::PROFILE)
      .write(profile_.accounts, metrics::PROFILE)
      .write(profile_.accounts_ack, metrics::PROFILE)
      .write(profile_.orders, metrics::PROFILE)
      .write(profile_.orders_ack, metrics::PROFILE)
      .write(profile_.fills, metrics::PROFILE)
      .write(profile_.fills_ack, metrics::PROFILE)
      .write(profile_.create_order, metrics::PROFILE)
      .write(profile_.create_order_ack, metrics::PROFILE)
      .write(profile_.cancel_order, metrics::PROFILE)
      .write(profile_.cancel_order_ack, metrics::PROFILE)
      .write(profile_.cancel_all_orders, metrics::PROFILE)
      .write(profile_.cancel_all_orders_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

uint16_t OrderEntry::operator()(
    const Event<CreateOrder> &event, const oms::Order &order, const std::string_view &request_id) {
  create_order(event, order, request_id);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<ModifyOrder> &,
    const oms::Order &,
    [[maybe_unused]] const std::string_view &request_id,
    [[maybe_unused]] const std::string_view &previous_request_id) {
  throw oms::NotSupportedException();
}

uint16_t OrderEntry::operator()(
    const Event<CancelOrder> &event,
    const oms::Order &order,
    const std::string_view &request_id,
    const std::string_view &previous_request_id) {
  cancel_order(event, order, request_id, previous_request_id);
  return stream_id_;
}

uint16_t OrderEntry::operator()(
    const Event<CancelAllOrders> &event, const std::string_view &request_id) {
  cancel_all_orders(event, request_id);
  return stream_id_;
}

void OrderEntry::operator()(const core::web::Client::Connected &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void OrderEntry::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void OrderEntry::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void OrderEntry::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
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
    case OrderEntryState::ACCOUNTS:
      get_accounts();
      return 1;
    case OrderEntryState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

// accounts

void OrderEntry::get_accounts() {
  profile_.accounts([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/spot/v3/accounts"sv;
    auto headers = security_.create_signature(method, path, {});
    log::debug(R"(HERE headers="{}")"sv, headers);
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto sequence = download_.sequence();
    connection_(
        "accounts"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_accounts_ack(event, sequence);
        });
  });
}

void OrderEntry::get_accounts_ack(
    const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.accounts_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = OrderEntryState::ACCOUNTS;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      response.expect(core::http::Status::OK);
      core::json::Buffer buffer(decode_buffer_);
      auto accounts = core::json::Parser::create<json::Accounts>(body, buffer);
      server::Trace event(trace_info, accounts);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void OrderEntry::operator()(const server::Trace<json::Accounts> &event) {
  auto &[trace_info, accounts] = event;
  log::info<4>("accounts={}"sv, accounts);
  for (auto &item : accounts.data) {
    log::info<2>("item={}"sv, item);
    FundsUpdate funds_update{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .currency = item.currency,
        .balance = item.balance,
        .hold = item.hold,
        .external_account = item.id,
    };
    server::create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  }
}

// create-order

void OrderEntry::create_order(
    const Event<CreateOrder> &event, const oms::Order &order, const std::string_view &request_id) {
  profile_.create_order([&]() {
    if (!ready())
      throw oms::NotReadyException();
    /*
    auto &[message_info, create_order] = event;
    auto method = core::http::Method::POST;
    auto path = "/api/v1/orders"sv;
    auto side = json::map(create_order.side).as_raw_text();
    auto type = json::map(create_order.order_type).as_raw_text();
    auto remark = ""sv;
    auto stp = Flags::self_trade_prevention();
    auto trade_type = "TRADE"sv;
    auto time_in_force = json::map(create_order.time_in_force).as_raw_text();
    if (create_order.execution_instruction != ExecutionInstruction{})
      throw oms::RejectedException(Origin::GATEWAY, Error::INVALID_EXECUTION_INSTRUCTION);
    // XXX use encode buffer
    auto body = fmt::format(
        R"({{)"
        R"("clientOid":"{}",)"
        R"("side":"{}",)"
        R"("symbol":"{}",)"
        R"("type":"{}",)"
        R"("remark":"{}",)"
        R"("stp":"{}",)"
        R"("tradeType":"{}",)"
        R"("price":{:.{}f},)"
        R"("size":{:.{}f},)"
        R"("timeInForce":"{}")"
        R"(}})"sv,
        request_id,
        side,
        create_order.symbol,
        type,
        remark,
        stp,
        trade_type,
        utils::Number{create_order.price, order.price_decimals},
        utils::Number{create_order.quantity, order.quantity_decimals},
        time_in_force);
    log::debug(R"(body="{}")"sv, body);
    auto headers = security_.create_signature_api_v2(method, path, {}, body);
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = core::http::ContentType::JSON,
        .headers = headers,
        .body = body,
        .quality_of_service = core::web::QualityOfService::IMMEDIATE,
    };
    connection_(
        request_id,
        request,
        [this, user_id = message_info.source, order_id = create_order.order_id](
            [[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          uint32_t version = 1;
          create_order_ack(event, user_id, order_id, version);
        });
    */
  });
}

void OrderEntry::create_order_ack(
    const server::Trace<core::web::Response> &event,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  profile_.create_order_ack([&]() {
    auto &[trace_info, response] = event;
    log::debug("user_id={}, order_id={}, version={}"sv, user_id, order_id, version);
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      core::json::Buffer buffer(decode_buffer_);
      /*
      auto create_order_ack = core::json::Parser::create<json::CreateOrderAck>(body, buffer);
      server::Trace event(trace_info, create_order_ack);
      (*this)(event, status, user_id, order_id, version);
      */
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::GATEWAY,
          .status = e.request_status(),
          .error = e.error(),
          .text = e.what(),
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
      }
    }
  });
}

/*
void OrderEntry::operator()(
    const server::Trace<json::CreateOrderAck> &event,
    core::http::Status status,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  auto &[trace_info, create_order_ack] = event;
  log::info<1>(
      "create_order_ack={}, user_id={}, order_id={}, version={}"sv,
      create_order_ack,
      user_id,
      order_id,
      version);
  auto category = core::http::to_category(status);
  switch (category) {
    case core::http::Category::SUCCESS: {  // 2xx
      if (create_order_ack.code == 200000) {
        oms::Response response{
            .type = RequestType::CREATE_ORDER,
            .origin = Origin::EXCHANGE,
            .status = RequestStatus::ACCEPTED,
            .error = {},
            .text = create_order_ack.msg,
            .version = version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        oms::OrderUpdate order_update{
            .account = security_.get_account(),
            .exchange = Flags::exchange(),
            .symbol = {},
            .side = {},
            .position_effect = {},
            .max_show_quantity = NaN,
            .order_type = {},
            .time_in_force = {},
            .execution_instruction = {},
            .order_template = {},
            .create_time_utc = {},
            .update_time_utc = {},
            .external_account = {},
            .external_order_id = create_order_ack.data.order_id,
            .status = OrderStatus::ACCEPTED,
            .quantity = NaN,
            .price = NaN,
            .stop_price = NaN,
            .remaining_quantity = NaN,
            .traded_quantity = NaN,
            .average_traded_price = NaN,
            .last_traded_quantity = NaN,
            .last_traded_price = NaN,
            .last_liquidity = {},
        };
        if (shared_.update_order(
                user_id,
                order_id,
                stream_id_,
                trace_info,
                response,
                order_update,
                []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
        }
      } else {
        oms::Response response{
            .type = RequestType::CREATE_ORDER,
            .origin = Origin::EXCHANGE,
            .status = RequestStatus::REJECTED,
            .error = json::guess_error(create_order_ack.code),
            .text = create_order_ack.msg,
            .version = version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        if (shared_.update_order(
                user_id,
                order_id,
                stream_id_,
                trace_info,
                response,
                []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
        }
      }
      break;
    }
    case core::http::Category::CLIENT_ERROR: {
      oms::Response response{
          .type = RequestType::CREATE_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::REJECTED,
          .error = json::guess_error(create_order_ack.code),
          .text = create_order_ack.msg,
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
      }
      break;
    }
    default:
      break;
  }
}
*/

// cancel-order

void OrderEntry::cancel_order(
    const Event<CancelOrder> &event,
    const oms::Order &order,
    [[maybe_unused]] const std::string_view &request_id,
    [[maybe_unused]] const std::string_view &previous_request_id) {
  profile_.cancel_order([&]() {
    if (!ready())
      throw oms::NotReadyException();
    if (std::empty(order.external_order_id))
      throw oms::RejectedException(Origin::GATEWAY, Error::UNKNOWN_EXTERNAL_ORDER_ID);
    auto &[message_info, cancel_order] = event;
    auto method = core::http::Method::DELETE;
    auto path = fmt::format("/api/v1/orders/{}"sv, order.external_order_id);
    auto headers = security_.create_signature(method, path, {});
    core::web::Request request{
        .method = method,
        .path = path,
        .query = {},
        .accept = core::http::Accept::JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = core::web::QualityOfService::IMMEDIATE,
    };
    connection_(
        request_id,
        request,
        [this,
         user_id = message_info.source,
         order_id = cancel_order.order_id,
         version = cancel_order.version]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          cancel_order_ack(event, user_id, order_id, version);
        });
  });
}

void OrderEntry::cancel_order_ack(
    const server::Trace<core::web::Response> &event,
    const uint8_t user_id,
    const uint32_t order_id,
    const uint32_t version) {
  profile_.cancel_order_ack([&]() {
    auto &[trace_info, response] = event;
    log::debug("user_id={}, order_id={}, version={}"sv, user_id, order_id, version);
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      core::json::Buffer buffer(decode_buffer_);
      /*
      auto cancel_order_ack = core::json::Parser::create<json::CancelOrderAck>(body, buffer);
      server::Trace event(trace_info, cancel_order_ack);
      (*this)(event, status, user_id, order_id, version);
      */
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::GATEWAY,
          .status = e.request_status(),
          .error = e.error(),
          .text = e.what(),
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv,
            user_id,
            order_id,
            version);
      }
    }
  });
}

/*
void OrderEntry::operator()(
    const server::Trace<json::CancelOrderAck> &event,
    core::http::Status status,
    uint8_t user_id,
    uint32_t order_id,
    uint32_t version) {
  auto &[trace_info, cancel_order_ack] = event;
  log::info<1>(
      "cancel_order_ack={}, user_id={}, order_id={}, version={}"sv,
      cancel_order_ack,
      user_id,
      order_id,
      version);
  auto category = core::http::to_category(status);
  switch (category) {
    case core::http::Category::SUCCESS: {  // 2xx
      if (cancel_order_ack.code == 200000) {
        std::string_view external_order_id;
        if (!std::empty(cancel_order_ack.data.cancelled_order_ids))
          external_order_id = cancel_order_ack.data.cancelled_order_ids[0];
        oms::Response response{
            .type = RequestType::CANCEL_ORDER,
            .origin = Origin::EXCHANGE,
            .status = RequestStatus::ACCEPTED,
            .error = {},
            .text = cancel_order_ack.msg,
            .version = version,
            .request_id = {},
            .quantity = NaN,
            .price = NaN,
        };
        oms::OrderUpdate order_update{
            .account = security_.get_account(),
            .exchange = Flags::exchange(),
            .symbol = {},
            .side = {},
            .position_effect = {},
            .max_show_quantity = NaN,
            .order_type = {},
            .time_in_force = {},
            .execution_instruction = {},
            .order_template = {},
            .create_time_utc = {},
            .update_time_utc = {},
            .external_account = {},
            .external_order_id = external_order_id,
            .status = OrderStatus::CANCELED,
            .quantity = NaN,
            .price = NaN,
            .stop_price = NaN,
            .remaining_quantity = NaN,
            .traded_quantity = NaN,
            .average_traded_price = NaN,
            .last_traded_quantity = NaN,
            .last_traded_price = NaN,
            .last_liquidity = {},
        };
        if (shared_.update_order(
                user_id,
                order_id,
                stream_id_,
                trace_info,
                response,
                order_update,
                []([[maybe_unused]] auto &order) {})) {
        } else {
          log::warn("Did not find order: user_id={}, order_id={}"sv, user_id, order_id);
        }
      }
      break;
    }
    case core::http::Category::CLIENT_ERROR: {  // 4xx
      oms::Response response{
          .type = RequestType::CANCEL_ORDER,
          .origin = Origin::EXCHANGE,
          .status = RequestStatus::REJECTED,
          .error = json::guess_error(cancel_order_ack.code),
          .text = cancel_order_ack.msg,
          .version = version,
          .request_id = {},
          .quantity = NaN,
          .price = NaN,
      };
      if (shared_.update_order(
              user_id,
              order_id,
              stream_id_,
              trace_info,
              response,
              []([[maybe_unused]] auto &order) {})) {
      } else {
        log::warn(
            "Did not find order: user_id={}, order_id={}, version={}"sv,
            user_id,
            order_id,
            version);
      }
      break;
    }
    default:
      break;
  }
}
*/

// cancel-all-orders

void OrderEntry::cancel_all_orders(
    const Event<CancelAllOrders> &event, [[maybe_unused]] const std::string_view &request_id) {
  profile_.cancel_all_orders([&]() {
    if (ready()) {
      auto method = core::http::Method::DELETE;
      auto path = "/api/v1/orders"sv;
      auto headers = security_.create_signature(method, path, {});
      core::web::Request request{
          .method = method,
          .path = path,
          .query = {},
          .accept = core::http::Accept::JSON,
          .content_type = {},
          .headers = headers,
          .body = {},
          .quality_of_service = core::web::QualityOfService::IMMEDIATE,
      };
      connection_(request_id, request, [this]([[maybe_unused]] auto &request_id, auto &response) {
        auto trace_info = server::create_trace_info();
        server::Trace event(trace_info, response);
        cancel_all_orders_ack(event);
      });
    } else {
      auto &[message_info, cancel_all_orders] = event;
      log::warn(
          R"(*** NOT CONNECTED! UNABLE TO CANCEL ALL ORDERS FOR ACCOUNT="{}")"sv,
          cancel_all_orders.account);
    }
  });
}

void OrderEntry::cancel_all_orders_ack(const server::Trace<core::web::Response> &event) {
  profile_.cancel_all_orders_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      core::json::Buffer buffer(decode_buffer_);
      /*
      auto cancel_all_orders_ack =
          core::json::Parser::create<json::CancelAllOrdersAck>(body, buffer);
      log::debug("cancel_all_orders_ack={}"sv, cancel_all_orders_ack);
      server::Trace event(trace_info, cancel_all_orders_ack);
      (*this)(event, status);
      */
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      // note! this event does not require a response
    }
  });
}

/*
void OrderEntry::operator()(
    const server::Trace<json::CancelAllOrdersAck> &event, core::http::Status status) {
  auto &[trace_info, cancel_all_orders_ack] = event;
  log::info<1>("cancel_all_orders_ack={}"sv, cancel_all_orders_ack);
  auto category = core::http::to_category(status);
  switch (category) {
    case core::http::Category::SUCCESS: {  // 2xx
      break;
    }
    case core::http::Category::CLIENT_ERROR: {  // 4xx
      break;
    }
    default:
      break;
  }
}
*/

}  // namespace okex
}  // namespace roq
