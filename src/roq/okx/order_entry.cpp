/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/order_entry.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/common.hpp"
#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/rest/client.hpp"

#include "roq/core/json/parser.hpp"

#include "roq/okx/json/map.hpp"
#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "dc"sv;

auto const SUPPORTS = Mask<SupportType>{};

size_t const MAX_DECODE_BUFFER_DEPTH = 2;

size_t const DOWNLOAD_TRADES_LIMIT = 100;
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
      .interface = settings.misc.test_local_interface,
      .proxy = settings.rest.proxy,
      .uris = {&uri, 1},
      .host = settings.rest.host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = {},
      .disconnect_on_idle_timeout = {},
      .connection = web::http::Connection::KEEP_ALIVE,
      // request
      .allow_pipelining = true,
      .request_timeout = settings.rest.request_timeout,
      // response
      .suspend_on_retry_after = {},
      // http
      .query = {},
      .user_agent = ROQ_PACKAGE_NAME,
      .ping_frequency = settings.rest.ping_freq,
      .ping_path = settings.rest.ping_path,
      // implementation
      .decode_buffer_size = settings.misc.decode_buffer_size,
      .encode_buffer_size = settings.misc.encode_buffer_size,
  };
  return web::rest::Client::create(handler, context, config);
}

struct create_metrics final : public utils::metrics::Factory {
  create_metrics(auto &settings, auto &group, auto const &function) : utils::metrics::Factory{settings.app.name, group, function} {}
};

auto get_download_trades_lookback(auto &settings, auto download_trades_is_first) {
  if (download_trades_is_first) {
    if (!settings.download.trades_lookback_on_restart.count()) {
      return settings.download.trades_lookback_on_restart;
    }
  }
  return settings.download.trades_lookback;
}
}  // namespace

// === IMPLEMENTATION ===

OrderEntry::OrderEntry(Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, Request &request)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .balance = create_metrics(shared.settings, name_, "balance"sv),
          .balance_ack = create_metrics(shared.settings, name_, "balance_ack"sv),
          .positions = create_metrics(shared.settings, name_, "positions"sv),
          .positions_ack = create_metrics(shared.settings, name_, "positions_ack"sv),
          .orders_pending = create_metrics(shared.settings, name_, "orders_pending"sv),
          .orders_pending_ack = create_metrics(shared.settings, name_, "orders_pending_ack"sv),
          .fills = create_metrics(shared.settings, name_, "fills"sv),
          .fills_ack = create_metrics(shared.settings, name_, "fills_ack"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
      },
      account_{account}, shared_{shared}, request_{request} {
}

void OrderEntry::operator()(Event<Start> const &) {
  (*connection_).start();
}

void OrderEntry::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void OrderEntry::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if (ready() && !download_balance_) {
    if (request_.respond_balance < request_.request_balance) {
      log::info<1>("Download balance..."sv);
      get_balance();
      download_balance_ = true;
    }
  }
  if (ready() && !download_positions_) {
    if (request_.respond_positions < request_.request_positions) {
      log::info<1>("Download positions..."sv);
      get_positions();
      download_positions_ = true;
    }
  }
  if (ready() && !download_orders_) {
    if (request_.respond_orders < request_.request_orders) {
      log::info<1>("Download orders..."sv);
      get_orders_pending();
      get_fills();
      download_orders_ = true;
    }
  }
}

void OrderEntry::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.balance, metrics::Type::PROFILE)
      .write(profile_.balance_ack, metrics::Type::PROFILE)
      .write(profile_.positions, metrics::Type::PROFILE)
      .write(profile_.positions_ack, metrics::Type::PROFILE)
      .write(profile_.orders_pending, metrics::Type::PROFILE)
      .write(profile_.orders_pending_ack, metrics::Type::PROFILE)
      .write(profile_.fills, metrics::Type::PROFILE)
      .write(profile_.fills_ack, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY);
}

void OrderEntry::operator()(ConnectionStatus status) {
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

// web::rest::Client::Handler

void OrderEntry::operator()(Trace<web::rest::Client::Connected> const &) {
  (*this)(ConnectionStatus::READY);
}

void OrderEntry::operator()(Trace<web::rest::Client::Disconnected> const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_orders_ = false;
}

void OrderEntry::operator()(Trace<web::rest::Client::Latency> const &event) {
  auto &[trace_info, latency] = event;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

// balance

void OrderEntry::get_balance() {
  profile_.balance([&]() {
    auto method = web::http::Method::GET;
    auto path = shared_.api.simple.account_balance;
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
      get_balance_ack(event);
    };
    (*connection_)("balance"sv, request, callback);
  });
}

void OrderEntry::get_balance_ack(Trace<web::rest::Response> const &event) {
  profile_.balance_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&]([[maybe_unused]] auto &body) {
      /*
      json::BalanceAck balance_ack{body, decode_buffer_};
      Trace event_2{event, balance_ack};
      (*this)(event_2);
      */
      download_balance_ = false;
      request_.respond_balance = clock::get_system();  // ack
    };
    process_response(event, handle_error, handle_success);
  });
}

/*
void OrderEntry::operator()(Trace<json::BalanceAck> const &event) {
  auto &[trace_info, balance_ack] = event;
  log::info<4>("balance_ack={}"sv, balance_ack);
}
*/

// positions

void OrderEntry::get_positions() {
  profile_.positions([&]() {
    auto method = web::http::Method::GET;
    auto path = shared_.api.simple.account_positions;
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
      get_positions_ack(event);
    };
    (*connection_)("positions"sv, request, callback);
  });
}

void OrderEntry::get_positions_ack(Trace<web::rest::Response> const &event) {
  profile_.positions_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&](auto &body) {
      json::PositionsAck positions_ack{body, decode_buffer_};
      if (positions_ack.code == 0) {
        Trace event_2{event, positions_ack};
        (*this)(event_2);
      } else {
        handle_error(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(positions_ack.code), positions_ack.msg);
      }
      download_positions_ = false;
      request_.respond_positions = clock::get_system();  // ack
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntry::operator()(Trace<json::PositionsAck> const &event) {
  auto &[trace_info, positions_ack] = event;
  log::info<4>("positions_ack={}"sv, positions_ack);
  for (auto &item : positions_ack.data) {
    log::info<2>("item={}"sv, item);
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
}

// orders-pending

void OrderEntry::get_orders_pending() {
  profile_.orders_pending([&]() {
    auto method = web::http::Method::GET;
    auto path = shared_.api.simple.trade_orders_pending;
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
      get_orders_pending_ack(event);
    };
    (*connection_)("orders-pending"sv, request, callback);
  });
}

void OrderEntry::get_orders_pending_ack(Trace<web::rest::Response> const &event) {
  profile_.orders_pending_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&](auto &body) {
      json::OrdersPendingAck orders_pending_ack{body, decode_buffer_};
      if (orders_pending_ack.code == 0) {
        Trace event_2{event, orders_pending_ack};
        (*this)(event_2);
      } else {
        handle_error(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(orders_pending_ack.code), orders_pending_ack.msg);
      }
      download_orders_ = false;
      request_.respond_orders = clock::get_system();  // ack
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntry::operator()(Trace<json::OrdersPendingAck> const &event) {
  auto &[trace_info, orders_pending_ack] = event;
  log::info<4>("orders_pending_ack={}"sv, orders_pending_ack);
  for (auto &item : orders_pending_ack.data) {
    log::info<2>("item={}"sv, item);
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
        .create_time_utc = {},
        .update_time_utc = utils::safe_cast(item.u_time),
        .external_account = {},
        .external_order_id = item.ord_id,
        .client_order_id = {},
        .order_status = map(item.state),
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
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
    };
    if (shared_.update_order(item.cl_ord_id, stream_id_, trace_info, order_update, [&]([[maybe_unused]] auto &order) {})) {
    } else {
      log::warn("*** EXTERNAL ORDER ***"sv);
      log::warn("item={}"sv, item);
    }
  }
}

// fills

void OrderEntry::get_fills() {
  profile_.fills([&]() {
    auto now = clock::get_realtime<std::chrono::milliseconds>();
    auto lookback = get_download_trades_lookback(shared_.settings, download_trades_is_first_);
    log::info<1>("Download trades: lookback={}"sv, lookback);
    auto begin = std::chrono::duration_cast<std::chrono::milliseconds>(now - lookback);
    auto limit = shared_.settings.download.trades_limit ? shared_.settings.download.trades_limit : DOWNLOAD_TRADES_LIMIT;
    // XXX FIXME doesn't look like begin/end is actually being used
    auto body = fmt::format(
        R"({{)"
        R"("begin":{},)"
        R"("end":{},)"
        R"("limit":{})"
        R"(}})"sv,
        begin.count(),
        now.count(),
        limit);
    auto method = web::http::Method::GET;
    auto path = shared_.api.simple.trade_fills;
    auto headers = account_.create_headers(method, path, body);
    auto request = web::rest::Request{
        .method = method,
        .path = path,
        .query = {},
        .accept = web::http::Accept::APPLICATION_JSON,
        .content_type = web::http::ContentType::APPLICATION_JSON,
        .headers = headers,
        .body = body,
        .quality_of_service = {},
    };
    auto callback = [this]([[maybe_unused]] auto &request_id, auto &response) {
      TraceInfo trace_info;
      Trace event{trace_info, response};
      get_fills_ack(event);
    };
    (*connection_)("fills"sv, request, callback);
  });
}

void OrderEntry::get_fills_ack(Trace<web::rest::Response> const &event) {
  profile_.fills_ack([&]() {
    auto handle_error = [&](auto origin, auto status, auto error, auto const &text) {
      log::warn(R"(origin={}, error={}, status={}, text="{}")"sv, origin, error, status, text);
      // XXX WHAT ???
    };
    auto handle_success = [&](auto &body) {
      json::FillsAck fills_ack{body, decode_buffer_};
      if (fills_ack.code == 0) {
        Trace event_2{event, fills_ack};
        (*this)(event_2);
      } else {
        handle_error(Origin::EXCHANGE, RequestStatus::REJECTED, json::guess_error(fills_ack.code), fills_ack.msg);
      }
      // download_orders_ = false;
      // request_.respond_orders = clock::get_system();  // ack
      download_trades_is_first_ = false;
    };
    process_response(event, handle_error, handle_success);
  });
}

void OrderEntry::operator()(Trace<json::FillsAck> const &event) {
  auto &[trace_info, fills_ack] = event;
  log::info<4>("fills_ack={}"sv, fills_ack);
  for (auto &item : fills_ack.data) {
    log::info<2>("item={}"sv, item);
    auto side = map(item.side).template get<Side>();
    auto ref_data = shared_.get_ref_data(shared_.settings.exchange, item.inst_id);
    auto profit_loss_amount = utils::compute_profit_loss_amount(side, item.fill_sz, item.fill_px, ref_data.multiplier);
    auto fill = Fill{
        .exchange_time_utc = utils::safe_cast(item.fill_time),
        .external_trade_id = {},
        .quantity = item.fill_sz,
        .price = item.fill_px,
        .liquidity = map(item.exec_type),
        .commission_amount = item.fee,
        .commission_currency = item.fee_ccy,
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
        .side = side,
        .position_effect = {},
        .margin_mode = {},
        .quantity_type = {},
        .create_time_utc = utils::safe_cast(item.fill_time),
        .update_time_utc = utils::safe_cast(item.fill_time),
        .external_account = {},
        .external_order_id = item.ord_id,
        .client_order_id = {},
        .fills = {&fill, 1},
        .routing_id = {},
        .update_type = UpdateType::SNAPSHOT,
        .sending_time_utc = {},
        .user = {},
        .strategy_id = {},
    };
    create_trace_and_dispatch(handler_, trace_info, trade_update, true, SOURCE_NONE, item.cl_ord_id);
  }
}

// helpers

void OrderEntry::process_response(web::rest::Response const &response, auto error_handler, auto success_handler) {
  try {
    auto [status, category, body] = response.result();
    switch (category) {
      using enum web::http::Category;
      case UNKNOWN:
      case INFORMATIONAL_RESPONSE:
        response.expect(web::http::Status::OK);  // throws
        break;
      case SUCCESS:
        success_handler(body);
        break;
      case REDIRECTION:
        log::fatal("Unexpected: URL is being redirected"sv);
      case CLIENT_ERROR: {
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, text);
        break;
      }
      case SERVER_ERROR: {
        auto text = fmt::format("{}"sv, status);
        error_handler(Origin::EXCHANGE, RequestStatus::REJECTED, Error::UNKNOWN, text);
        break;
      }
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
