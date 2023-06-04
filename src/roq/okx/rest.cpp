/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/rest.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/web/rest/client_factory.hpp"

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "dc"sv;

auto const SUPPORTS = Mask<SupportType>{};
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.rest.uri;
  auto config = web::rest::Client::Config{
      // connection
      .interface = {},
      .uris = {&uri, 1},
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // proxy
      .proxy = settings.rest.proxy,
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .request_timeout = settings.rest.request_timeout,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = settings.rest.ping_path,
      // implementation
      .decode_buffer_size = settings.common.decode_buffer_size,
      .encode_buffer_size = settings.common.encode_buffer_size,
      .allow_pipelining = true,
  };
  return web::rest::ClientFactory::create(handler, context, config);
}

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(auto &settings, auto const &group, auto const &function)
      : core::metrics::Factory(settings.app.name, group, function) {}
};
}  // namespace

// === IMPLEMENTATION ===

Rest::Rest(
    Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)},
      connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_(shared.settings.common.decode_buffer_size),
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .orders = create_metrics(shared.settings, name_, "orders"sv),
          .orders_ack = create_metrics(shared.settings, name_, "orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      account_{account}, shared_{shared}, request_{request} {
}

void Rest::operator()(Event<Start> const &) {
  (*connection_).start();
}

void Rest::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void Rest::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if (ready() && !download_orders_) {
    if (request_.respond_orders < request_.request_orders) {
      log::info<1>("Download orders..."sv);
      get_orders();
      download_orders_ = true;
    }
  }
}

void Rest::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.orders, metrics::PROFILE)
      .write(profile_.orders_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

void Rest::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    TraceInfo trace_info;
    auto stream_status = StreamStatus{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS,
        .transport = Transport::TCP,
        .protocol = Protocol::HTTP,
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

void Rest::operator()(Trace<web::rest::Client::Connected> const &) {
  (*this)(ConnectionStatus::READY);
}

void Rest::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_orders_ = false;
}

void Rest::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void Rest::operator()(
    Trace<web::rest::Response> const &, [[maybe_unused]] uint64_t request_id, [[maybe_unused]] uint64_t opaque) {
}

// orders-pending

void Rest::get_orders() {
  profile_.orders([&]() {
    auto method = web::http::Method::GET;
    auto path = "/api/v5/trade/orders-pending"sv;
    auto headers = account_.create_headers(method, path, {});
    auto request = web::rest::Request{
        .method = method,
        .path = path,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = {},
        .headers = headers,
        .body = {},
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_orders_ack(event);
    };
    (*connection_)("orders"sv, request, callback);
  });
}

void Rest::get_orders_ack(Trace<web::rest::Response> const &event) {
  profile_.orders_ack([&]() {
    auto handle_success = [&](auto &body) {
      auto orders = json::Orders::create(body, decode_buffer_);
      Trace event_2{event, orders};
      (*this)(event_2);
      download_orders_ = false;
      request_.respond_orders = clock::get_system();  // ack
    };
    auto handle_error = [&]([[maybe_unused]] auto origin, [[maybe_unused]] auto status, auto error, auto text) {
      log::warn(R"(error={}, text="{}")"sv, error, text);
      // XXX WHAT ???
    };
    process_response(event, handle_success, handle_error);
  });
}

void Rest::operator()(Trace<json::Orders> const &event) {
  auto &[trace_info, orders] = event;
  log::info<4>("orders={}"sv, orders);
  for (auto &item : orders.data) {
    auto side = json::map(item.side);
    auto order_status = json::map(item.state);
    auto order_update = oms::OrderUpdate{
        .account = account_.get_name(),
        .exchange = shared_.settings.exchange,
        .symbol = item.inst_id,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = {},
        .time_in_force = {},
        .execution_instructions = {},
        .create_time_utc = {},
        .update_time_utc = utils::safe_cast(item.u_time),
        .external_account = {},
        .external_order_id = item.ord_id,
        .client_order_id = {},
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
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    if (shared_.update_order(
            item.cl_ord_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
    } else {
      log::warn("*** EXTERNAL ORDER ***"sv);
      log::warn("item={}"sv, item);
    }
  }
}

template <typename SuccessHandler, typename ErrorHandler>
void Rest::process_response(
    web::rest::Response const &response, SuccessHandler success_handler, ErrorHandler error_handler) {
  try {
    auto [status, category, body] = response.result();
    log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
    switch (category) {
      using enum web::http::Category;
      case SUCCESS:  // 2xx
        success_handler(body);
        break;
      case CLIENT_ERROR: {  // 4xx
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, text);
        break;
      }
      case SERVER_ERROR: {  // 5xx
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, text);
        break;
      }
      default:
        response.expect(web::http::Status::OK);  // throws
    }
  } catch (NetworkError &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::GATEWAY, e.request_status(), e.error(), e.what());
  } catch (std::exception &e) {
    log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    error_handler(Origin::EXCHANGE, RequestStatus::ERROR, Error::UNKNOWN, e.what());
  }
}
}  // namespace okx
}  // namespace roq
