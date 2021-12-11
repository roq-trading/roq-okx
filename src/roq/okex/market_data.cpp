/* Copyright (c) 2017-2022, Hans Erik Thrane */

#define ZLIB_CONST

#include "roq/okex/market_data.h"

#include <algorithm>

#include "roq/utils/mask.h"
#include "roq/utils/safe_cast.h"
#include "roq/utils/update.h"

#include "roq/core/back_emplacer.h"
#include "roq/core/charconv.h"

#include "roq/core/tools/exception.h"

#include "roq/core/metrics/factory.h"

#include "roq/okex/flags.h"

#include "roq/okex/json/utils.h"

#include <zlib.h>
#include "roq/core/debug.h"

using namespace std::literals;

namespace roq {
namespace okex {

namespace {
static std::string decode(const roq::span<std::byte const> &payload) {
  core::print_memory(payload);

  std::string result;
  std::vector<std::byte> buffer(4096);
  z_stream stream = {};
  if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
    throw RuntimeErrorException("Can't initialize inflate"sv);
  stream.avail_in = std::size(payload);
  stream.next_in = reinterpret_cast<decltype(stream.next_in)>(std::data(payload));
  try {
    int ret = Z_OK;
    do {
      stream.avail_out = std::size(buffer);
      stream.next_out = reinterpret_cast<decltype(stream.next_out)>(std::data(buffer));
      ret = inflate(&stream, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
      switch (ret) {
        case Z_NEED_DICT:
          throw RuntimeErrorException("Z_NEED_DICT"sv);
        case Z_DATA_ERROR:
          throw RuntimeErrorException("Z_DATA_ERROR"sv);
        case Z_MEM_ERROR:
          throw RuntimeErrorException("Z_MEM_ERROR"sv);
      }
      auto bytes = std::size(buffer) - stream.avail_out;
      log::debug(
          "append {} bytes: {}"sv,
          bytes,
          std::string_view{reinterpret_cast<char const *>(std::data(buffer)), bytes});
      result.append(reinterpret_cast<std::string::value_type const *>(std::data(buffer)), bytes);
      if (stream.avail_out == 0) {
        // XXX extend
        assert(false);
      }
    } while (stream.avail_out == 0);
    if (ret != Z_STREAM_END)
      throw RuntimeErrorException("NEED MORE DATA"sv);
  } catch (...) {
    inflateEnd(&stream);
    throw;
  }
  assert(stream.avail_in == 0);
  inflateEnd(&stream);
  return result;
}
}  // namespace

namespace {
static const auto NAME = "md"sv;
static const auto SUPPORTS = utils::Mask{
    SupportType::MARKET_STATUS,
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};

struct create_metrics final : public core::metrics::Factory {
  explicit create_metrics(const std::string_view &group, const std::string_view &function)
      : core::metrics::Factory(server::Flags::name(), group, function) {}
};

void emplace(MBPUpdate &result, double price, double size) {
  new (&result) MBPUpdate{
      .price = price,
      .quantity = size,
      .implied_quantity = NaN,
      .price_level = {},
      .number_of_orders = {},
  };
}
}  // namespace

MarketData::MarketData(
    Handler &handler, core::io::Context &context, uint32_t stream_id, Shared &shared)
    : handler_(handler), stream_id_(stream_id), name_(fmt::format("{}:{}"sv, stream_id_, NAME)),
      connection_(
          *this,
          context,
          core::URI{Flags::ws_uri()},
          {},  // query
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
      },
      latency_{
          .ping = create_metrics(name_, "ping"sv),
          .heartbeat = create_metrics(name_, "heartbeat"sv),
      },
      shared_(shared), download_({}, [this](auto state) { return download(state); }) {
}

void MarketData::operator()(const Event<Start> &) {
  connection_.start();
}

void MarketData::operator()(const Event<Stop> &) {
  connection_.stop();
}

void MarketData::operator()(const Event<Timer> &event) {
  auto now = event.value.now;
  connection_.refresh(now);
  /*
  if (connection_.ready()) {
    if (welcome_ && next_ping_ < now)
      send_ping(now);
    check_subscribe_queue(now);
  } else if (logon_timeout_.count() && logon_timeout_ < now) {
    assert(!welcome_);
    log::warn("Did not receive the welcome message, disconnecting now..."sv);
    connection_.close();
  }
  */
}

void MarketData::operator()(metrics::Writer &writer) {
  writer
      // counter
      .write(counter_.disconnect, metrics::COUNTER)
      // profile
      .write(profile_.parse, metrics::PROFILE)
      .write(profile_.error, metrics::PROFILE)
      .write(profile_.subscribe, metrics::PROFILE)
      .write(profile_.unsubscribe, metrics::PROFILE)
      // latency
      .write(latency_.ping, metrics::LATENCY)
      .write(latency_.heartbeat, metrics::LATENCY);
}

void MarketData::update_subscriptions(std::vector<std::string> &symbols) {
  assert(&symbols != &symbols_);
  auto max_size = Flags::ws_max_subscriptions_per_stream();
  auto offset = std::size(symbols_);
  if (max_size <= offset)
    return;
  if (std::empty(symbols))
    return;
  symbols_.reserve(max_size);
  auto length = std::min(max_size - offset, std::size(symbols));
  assert(length > 0);
  for (size_t i = 0; i < length; ++i) {
    symbols_.emplace_back(symbols.back());
    symbols.pop_back();
  }
  assert(length == (std::size(symbols_) - offset));
  if (ready())
    subscribe({&symbols_[offset], length});
}

void MarketData::operator()(const core::web::ClientSocket::Connected &) {
}

void MarketData::operator()(const core::web::ClientSocket::Disconnected &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  // experimental
  shared_.mbp_collector.clear();  // XXX HANS this is SHARED !!!
}

void MarketData::operator()(const core::web::ClientSocket::Ready &) {
  (*this)(ConnectionStatus::DOWNLOADING);
  download_.begin();
}

void MarketData::operator()(const core::web::ClientSocket::Close &) {
}

void MarketData::operator()(const core::web::ClientSocket::Latency &latency) {
  auto trace_info = server::create_trace_info();
  ExternalLatency external_latency{
      .stream_id = stream_id_,
      .latency = latency.sample,
  };
  server::create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(const core::web::ClientSocket::Text &text) {
  log::fatal(R"(Unexpected: message="{}")"sv);
  parse(text.payload);
}

void MarketData::operator()(const core::web::ClientSocket::Binary &binary) {
  auto message = decode(binary.payload);
  parse(message);
}

void MarketData::operator()(ConnectionStatus status) {
  if (utils::update(status_, status)) {
    auto trace_info = server::create_trace_info();
    StreamStatus stream_status{
        .stream_id = stream_id_,
        .account = {},
        .supports = SUPPORTS.get(),
        .status = status_,
        .type = StreamType::WEB_SOCKET,
        .priority = Priority::PRIMARY,
    };
    log::info("stream_status={}"sv, stream_status);
    server::create_trace_and_dispatch(handler_, trace_info, stream_status);
  }
}

uint32_t MarketData::download(MarketDataState state) {
  switch (state) {
    case MarketDataState::UNDEFINED:
      assert(false);
      break;
    case MarketDataState::SUBSCRIBE:
      subscribe(symbols_);
      return {};
    case MarketDataState::DONE:
      (*this)(ConnectionStatus::READY);
      return {};
  }
  assert(false);
  return {};
}

void MarketData::subscribe(const roq::span<std::string> &symbols) {
  if (std::empty(symbols))
    return;
  subscribe("spot/ticker"sv, symbols);
}

void MarketData::subscribe(const std::string_view &channel, const roq::span<std::string> &symbols) {
  assert(!std::empty(symbols));
  // note! can't exceed 4k bytes
  auto message = fmt::format(R"({{)"
                             R"("op":"subscribe",)"
                             R"("args":["spot/ticker:ETH-USDT"])"
                             R"(}})"sv);
  log::debug("message={}"sv, message);
  connection_.send_text(message);
}

void MarketData::parse(const std::string_view &message) {
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

void MarketData::operator()(server::Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::info<1>("event={{trace_info={}, error={}}}"sv, trace_info, error);
  });
}

void MarketData::operator()(server::Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("event={{trace_info={}, subscribe={}}}"sv, trace_info, subscribe);
  });
}

void MarketData::operator()(server::Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("event={{trace_info={}, unsubscribe={}}}"sv, trace_info, unsubscribe);
  });
}

}  // namespace okex
}  // namespace roq
