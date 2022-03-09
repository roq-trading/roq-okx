/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/rest.hpp"

#include <algorithm>
#include <utility>

#include "roq/utils/mask.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/core/back_emplacer.hpp"
#include "roq/core/charconv.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/core/metrics/factory.hpp"

#include "roq/okx/flags.hpp"

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

namespace {
const auto NAME = "dc"sv;

const auto SUPPORTS = utils::Mask<SupportType>{};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

auto create_connection(auto &handler, auto &context) {
  auto uri = Flags::rest_uri();
  core::web::Client::Config config{
      .decode_buffer_size = Flags::decode_buffer_size(),
      .encode_buffer_size = Flags::encode_buffer_size(),
      .validate_certificate = server::Flags::tls_validate_certificate(),
      .uris = {&uri, 1},
      .proxy = Flags::rest_proxy(),
      .user_agent = ROQ_PACKAGE_NAME,
      .connection = core::http::Connection::KEEP_ALIVE,
      .allow_pipelining = true,
      .request_timeout = Flags::rest_request_timeout(),
      .ping_frequency = Flags::rest_ping_freq(),
      .ping_path = Flags::rest_ping_path(),
  };
  return core::web::Client{handler, context, config};
}
}  // namespace

Rest::Rest(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared,
    Request &request)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(create_connection(*this, context)), decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .orders = create_metrics(name_, "orders"sv),
          .orders_ack = create_metrics(name_, "orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared), request_(request) {
}

void Rest::operator()(const Event<Start> &) {
  connection_.start();
}

void Rest::operator()(const Event<Stop> &) {
  connection_.stop();
}

void Rest::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
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
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::REST,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

void Rest::operator()(const core::web::Client::Connected &) {
  (*this)(ConnectionStatus::READY);
}

void Rest::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_orders_ = false;
}

void Rest::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

// orders-pending

void Rest::get_orders() {
  profile_.orders([&]() {
    auto method = core::http::Method::GET;
    auto path = "/api/v5/trade/orders-pending"sv;
    auto headers = security_.create_headers(method, path, {});
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
    connection_("orders"sv, request, [this]([[maybe_unused]] auto &request_id, auto &response) {
      auto trace_info = server::create_trace_info();
      server::Trace event(trace_info, response);
      get_orders_ack(event);
    });
  });
}

void Rest::get_orders_ack(const server::Trace<core::web::Response> &event) {
  profile_.orders_ack([&]() {
    auto &[trace_info, response] = event;
    try {
      auto [status, category, body] = response.result();
      // log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      switch (category) {
        // using enum core::http::Category;  // XXX clang13
        case core::http::Category::UNKNOWN:
          log::fatal(R"(Unexpected: status={}, body="{}")"sv, status, body);
          break;
        case core::http::Category::INFORMATIONAL_RESPONSE:
          log::fatal(R"(Unexpected: status={}, body="{}")"sv, status, body);
          break;
        case core::http::Category::SUCCESS:
          break;
        case core::http::Category::REDIRECTION:
          log::fatal(R"(Unexpected: status={}, body="{}")"sv, status, body);
          break;
        case core::http::Category::CLIENT_ERROR:
          log::fatal(R"(Unexpected: status={}, body="{}")"sv, status, body);
          break;
        case core::http::Category::SERVER_ERROR:
          log::fatal(R"(Unexpected: status={}, body="{}")"sv, status, body);
          break;
      }
      // log::debug(R"(body="{}")"sv, body);
      core::json::Buffer buffer(decode_buffer_);
      auto orders = core::json::Parser::create<json::Orders>(body, buffer);
      server::Trace event(trace_info, orders);
      (*this)(event);
      download_orders_ = false;
      request_.respond_orders = core::clock::GetSystem();
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
    }
  });
}

void Rest::operator()(const server::Trace<json::Orders> &event) {
  auto &[trace_info, orders] = event;
  log::info<4>("orders={}"sv, orders);
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
        .update_type = UpdateType::SNAPSHOT,
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
}

}  // namespace okx
}  // namespace roq
