/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/drop_copy.h"

#include <algorithm>
#include <utility>

#include "roq/utils/mask.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/json/parser.h"

#include "roq/core/metrics/factory.h"

#include "roq/okx/flags.h"

using namespace std::literals;

namespace roq {
namespace okx {

namespace {
static const auto NAME = "dc"sv;

static const auto SUPPORTS = utils::Mask{
    SupportType::REFERENCE_DATA,
    SupportType::MARKET_STATUS,
};

static const auto ALLOW_PIPELINING = true;

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};
}  // namespace

DropCopy::DropCopy(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
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
          .orders = create_metrics(name_, "orders"sv),
          .orders_ack = create_metrics(name_, "orders_ack"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
      },
      security_(security), shared_(shared),
      download_(Flags::rest_request_timeout(), [this](auto state) { return download(state); }) {
}

void DropCopy::operator()(const Event<Start> &) {
  connection_.start();
}

void DropCopy::operator()(const Event<Stop> &) {
  connection_.stop();
}

void DropCopy::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
}

void DropCopy::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.orders, metrics::PROFILE)
      .write(profile_.orders_ack, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY);
}

void DropCopy::operator()(ConnectionStatus status) {
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

void DropCopy::operator()(const core::web::Client::Connected &) {
  if (download_.downloading()) {
    download_.bump();
  } else {
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  }
}

void DropCopy::operator()(const core::web::Client::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  if (!download_.downloading())
    download_.reset();
}

void DropCopy::operator()(const core::web::Client::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

uint32_t DropCopy::download(DropCopyState state) {
  switch (state) {
    case DropCopyState::UNDEFINED:
      assert(false);
      break;
    case DropCopyState::ORDERS:
      get_orders();
      return 1;
    case DropCopyState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

// orders-pending

void DropCopy::get_orders() {
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
    auto sequence = download_.sequence();
    connection_(
        "orders"sv, request, [this, sequence]([[maybe_unused]] auto &request_id, auto &response) {
          auto trace_info = server::create_trace_info();
          server::Trace event(trace_info, response);
          get_orders_ack(event, sequence);
        });
  });
}

void DropCopy::get_orders_ack(const server::Trace<core::web::Response> &event, uint32_t sequence) {
  profile_.orders_ack([&]() {
    auto &[trace_info, response] = event;
    auto state = DropCopyState::ORDERS;
    try {
      auto [status, category, body] = response.result();
      log::debug(R"(status={}, category={}, body="{}")"sv, status, category, body);
      if (download_.skip(sequence, state)) {
        log::info("Download state={} has already been processed"sv, state);
        return;
      }
      switch (category) {
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
      log::debug(R"(body="{}")"sv, body);
      core::json::Buffer buffer(decode_buffer_);
      auto orders = core::json::Parser::create<json::Orders>(body, buffer);
      server::Trace event(trace_info, orders);
      (*this)(event);
      download_.check(state);
    } catch (core::NetworkError &e) {
      log::warn(R"(Exception type={}, what="{}")"sv, typeid(e).name(), e.what());
      download_.retry(state);
    }
  });
}

void DropCopy::operator()(const server::Trace<json::Orders> &event) {
  auto &[trace_info, orders] = event;
  log::info<4>("orders={}"sv, orders);
}

}  // namespace okx
}  // namespace roq
