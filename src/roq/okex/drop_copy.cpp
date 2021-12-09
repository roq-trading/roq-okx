/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/drop_copy.h"

#include "roq/utils/mask.h"
#include "roq/utils/update.h"

#include "roq/core/metrics/factory.h"

#include "roq/core/json/buffer.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static const auto NAME = "ex"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::ORDER,
    SupportType::TRADE,
    SupportType::FUNDS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

static OrderStatus compute_order_status(
    json::OrderChangeType change_type, json::OrderStatus status, double remain_size) {
  switch (change_type) {
    case json::OrderChangeType::UNDEFINED:
      break;
    case json::OrderChangeType::UNKNOWN:
      break;
    case json::OrderChangeType::OPEN:
      break;
    case json::OrderChangeType::MATCH:
    case json::OrderChangeType::FILLED:
      return utils::compare(remain_size, 0.0) == 0 ? OrderStatus::COMPLETED : OrderStatus::WORKING;
    case json::OrderChangeType::CANCELED:
      return OrderStatus::CANCELED;
  }
  return json::map(status);
}
}  // namespace

DropCopy::DropCopy(
    Handler &handler,
    core::io::Context &context,
    uint16_t stream_id,
    Security &security,
    Shared &shared,
    const std::string_view &uri,
    const std::string_view &query,
    std::chrono::nanoseconds ping_frequency)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
          *this,
          context,
          core::URI{uri},
          query,
          Flags::ws_ping_freq(),
          Flags::decode_buffer_size(),
          Flags::encode_buffer_size(),
          []() { return std::string(); }),
      ping_frequency_(ping_frequency), decode_buffer_(Flags::decode_buffer_size()),
      counter_{
          .disconnect = create_metrics(name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(name_, "parse"sv),
          .welcome = create_metrics(name_, "welcome"sv),
          .error = create_metrics(name_, "error"sv),
          .pong = create_metrics(name_, "pong"sv),
          .ack = create_metrics(name_, "ack"sv),
          .account_balance = create_metrics(name_, "account_balance"sv),
          .order_change = create_metrics(name_, "order_change"sv),
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      security_(security), shared_(shared),
      download_({}, [this](auto state) { return download(state); }) {
}

bool DropCopy::ready() const {
  return connection_.ready();
}

void DropCopy::operator()(const Event<Start> &) {
  connection_.start();
}

void DropCopy::operator()(const Event<Stop> &) {
  connection_.stop();
}

void DropCopy::operator()(const Event<Timer> &event) {
  connection_.refresh(event.value.now);
}

void DropCopy::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.welcome, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.pong, metrics::PROFILE)
      .write(profile_.ack, metrics::PROFILE)
      .write(profile_.account_balance, metrics::PROFILE)
      .write(profile_.order_change, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void DropCopy::operator()(const core::web::ClientSocket::Connected &) {
  assert(logon_timeout_.count() == 0);
  auto now = core::get_system_clock();
  logon_timeout_ = now + Flags::ws_request_timeout();
}

void DropCopy::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  ready_ = false;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  welcome_ = false;
  logon_timeout_ = {};
  next_ping_ = {};
}

void DropCopy::operator()(const core::web::ClientSocket::Ready &) {
  // note! wait for welcome
}

void DropCopy::operator()(const core::web::ClientSocket::Close &) {
}

void DropCopy::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void DropCopy::operator()(const core::web::ClientSocket::Text &text) {
  parse(text.payload);
}

void DropCopy::operator()(const core::web::ClientSocket::Binary &) {
  log::fatal("Unexpected"sv);
}

void DropCopy::operator()(ConnectionStatus status) {
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

uint32_t DropCopy::download(DropCopyState state) {
  switch (state) {
    case DropCopyState::UNDEFINED:
      assert(false);
      break;
    case DropCopyState::SUBSCRIBE:
      subscribe();
      return {};
    case DropCopyState::DONE:
      (*this)(ConnectionStatus::READY);
      assert(!ready_);
      ready_ = true;
      return {};
  }
  assert(false);
  return {};
}

void DropCopy::subscribe() {
  subscribe("/account/balance"sv);
  subscribe("/spotMarket/tradeOrders"sv);
}

void DropCopy::subscribe(const std::string_view &topic) {
  auto now = core::get_system_clock();
  auto message = fmt::format(
      R"({{)"
      R"("id":"{}",)"
      R"("type":"subscribe",)"
      R"("privateChannel":true,)"
      R"("topic":"{}",)"
      R"("response":true)"
      R"(}})"sv,
      now.count(),
      topic);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void DropCopy::parse(const std::string_view &message) {
  profile_.parse([&]() {
    try {
      auto trace_info = server::create_trace_info();
      core::json::Buffer buffer(decode_buffer_);
      json::Parser::dispatch(*this, message, buffer, trace_info);
    } catch (...) {
      log::warn(R"(message="{}")"sv, message);
      core::tools::UnhandledException::terminate();
    }
  });
}

void DropCopy::operator()(server::Trace<json::Welcome> const &event) {
  profile_.welcome([&]() {
    auto &[trace_info, welcome] = event;
    log::info<1>("event={{trace_info={}, welcome={}}}"sv, trace_info, welcome);
    welcome_ = true;
    (*this)(ConnectionStatus::DOWNLOADING);
    download_.begin();
  });
}

void DropCopy::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    // XXX HANS DEBUG
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
    // log::fatal("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void DropCopy::operator()(server::Trace<json::Pong> const &event) {
  profile_.pong([&]() {
    auto &[trace_info, pong] = event;
    log::info<4>("event={{trace_info={}, pong={}}}"sv, trace_info, pong);
  });
}

void DropCopy::operator()(server::Trace<json::Ack> const &event) {
  profile_.ack([&]() {
    auto &[trace_info, ack] = event;
    log::info<2>("event={{trace_info={}, ack={}}}"sv, trace_info, ack);
    log::debug("ack={}"sv, ack);
  });
}

void DropCopy::operator()(server::Trace<json::Snapshot> const &) {
}

void DropCopy::operator()(server::Trace<json::Ticker> const &) {
}

void DropCopy::operator()(server::Trace<json::Level2> const &) {
}

void DropCopy::operator()(server::Trace<json::AccountBalance> const &event) {
  profile_.account_balance([&]() {
    auto &[trace_info, account_balance] = event;
    log::info<1>("event={{trace_info={}, account_balance={}}}"sv, trace_info, account_balance);
    log::debug("account_balance={}"sv, account_balance);
    auto &data = account_balance.data;
    FundsUpdate funds_update{
        .stream_id = stream_id_,
        .account = security_.get_account(),
        .currency = data.currency,
        .balance = data.total,  // XXX HANS or data.available?
        .hold = data.hold,
        .external_account = {},
    };
    server::create_trace_and_dispatch(handler_, trace_info, funds_update, true);
  });
}

void DropCopy::operator()(server::Trace<json::OrderChange> const &event) {
  profile_.order_change([&]() {
    // auto &[trace_info, order_change] = event;
    auto &trace_info = event.trace_info;
    auto &order_change = event.value;
    log::info<1>("event={{trace_info={}, order_change={}}}"sv, trace_info, order_change);
    log::debug("order_change={}"sv, order_change);
    auto &data = order_change.data;
    auto order_type = json::map(data.order_type);
    auto side = json::map(data.side);
    auto order_status = compute_order_status(data.type, data.status, data.remain_size);
    auto is_match = data.type == json::OrderChangeType::MATCH;
    auto last_traded_quantity = is_match ? data.match_size : NaN;
    auto last_traded_price = is_match ? data.match_price : NaN;
    auto last_liquidity = json::map(data.liquidity);
    oms::OrderUpdate order_update{
        .account = security_.get_account(),
        .exchange = Flags::exchange(),
        .symbol = data.symbol,
        .side = side,
        .position_effect = {},
        .max_show_quantity = NaN,
        .order_type = order_type,
        .time_in_force = {},
        .execution_instruction = {},
        .order_template = {},
        .create_time_utc = data.order_time,
        .update_time_utc = data.ts,
        .external_account = {},
        .external_order_id = data.order_id,
        .status = order_status,
        .quantity = data.size,
        .price = data.price,
        .stop_price = NaN,
        .remaining_quantity = data.remain_size,
        .traded_quantity = data.filled_size,
        .average_traded_price = NaN,  // note! not available
        .last_traded_quantity = last_traded_quantity,
        .last_traded_price = last_traded_price,
        .last_liquidity = last_liquidity,
    };
    if (shared_.update_order(
            data.client_oid, stream_id_, trace_info, order_update, [&](auto &order) {
              if (is_match) {
                Fill fill{
                    .external_trade_id = data.trade_id,
                    .quantity = last_traded_quantity,
                    .price = last_traded_price,
                    .liquidity = last_liquidity,
                };
                TradeUpdate trade_update{
                    .stream_id = stream_id_,
                    .account = order.account,
                    .order_id = order.order_id,
                    .exchange = order.exchange,
                    .symbol = order.symbol,
                    .side = order.side,
                    .position_effect = order.position_effect,
                    .create_time_utc = {},
                    .update_time_utc = data.ts,
                    .external_account = {},
                    .external_order_id = order.external_order_id,
                    .fills = {&fill, 1},
                    .routing_id = order.routing_id,
                };
                server::create_trace_and_dispatch(
                    handler_, trace_info, trade_update, true, order.user_id);
              }
            })) {
    } else {
      log::warn("*** EXTERNAL ORDER ***"sv);
      log::warn("order_change={}"sv, order_change);
    }
  });
}

}  // namespace okex
}  // namespace roq
