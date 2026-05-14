/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/market_data.hpp"

#include <algorithm>
#include <utility>

#include "roq/mask.hpp"

#include "roq/utils/safe_cast.hpp"
#include "roq/utils/update.hpp"

#include "roq/utils/exceptions/unhandled.hpp"

#include "roq/utils/metrics/factory.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/okx/json/map.hpp"
#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const NAME = "md"sv;

auto const SUPPORTS = Mask{
    SupportType::TOP_OF_BOOK,
    SupportType::MARKET_BY_PRICE,
    SupportType::TRADE_SUMMARY,
    SupportType::STATISTICS,
};

size_t const MAX_DECODE_BUFFER_DEPTH = 2;
}  // namespace

// === HELPERS ===

namespace {
auto create_name(auto stream_id) {
  return fmt::format("{}:{}"sv, stream_id, NAME);
}

auto create_connection(auto &handler, auto &settings, auto &context) {
  auto uri = settings.ws.public_uri;
  auto config = web::socket::Client::Config{
      // connection
      .interface = settings.misc.test_local_interface,
      .uris = {&uri, 1},
      .host = settings.ws.public_host,
      .validate_certificate = settings.net.tls_validate_certificate,
      // connection manager
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = settings.net.disconnect_on_idle_timeout,
      .always_reconnect = true,
      // proxy
      .proxy = {},
      // http
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
}  // namespace

// === IMPLEMENTATION ===

MarketData::MarketData(Handler &handler, io::Context &context, uint16_t stream_id, Account &account, Shared &shared, size_t index)
    : handler_{handler}, stream_id_{stream_id}, name_{create_name(stream_id_)}, index_{index}, connection_{create_connection(*this, shared.settings, context)},
      decode_buffer_{shared.settings.misc.decode_buffer_size, MAX_DECODE_BUFFER_DEPTH},
      counter_{
          .disconnect = create_metrics(shared.settings, name_, "disconnect"sv),
      },
      profile_{
          .parse = create_metrics(shared.settings, name_, "parse"sv),
          .error = create_metrics(shared.settings, name_, "error"sv),
          .subscribe = create_metrics(shared.settings, name_, "subscribe"sv),
          .unsubscribe = create_metrics(shared.settings, name_, "unsubscribe"sv),
          .login = create_metrics(shared.settings, name_, "login"sv),
          .estimated_price = create_metrics(shared.settings, name_, "estimated_price"sv),
          .price_limit = create_metrics(shared.settings, name_, "price_limit"sv),
          .mark_price = create_metrics(shared.settings, name_, "mark_price"sv),
          .tickers = create_metrics(shared.settings, name_, "tickers"sv),
          .trades = create_metrics(shared.settings, name_, "trades"sv),
          .bbo_tbt = create_metrics(shared.settings, name_, "bbo_tbt"sv),
          .books_l2_tbt = create_metrics(shared.settings, name_, "books_l2_tbt"sv),
          .index_tickers = create_metrics(shared.settings, name_, "index_tickers"sv),
          .funding_rate = create_metrics(shared.settings, name_, "funding_rate"sv),
      },
      latency_{
          .ping = create_metrics(shared.settings, name_, "ping"sv),
          .heartbeat = create_metrics(shared.settings, name_, "heartbeat"sv),
      },
      account_{account}, shared_{shared}, download_{{}, [this](auto state) { return download(state); }} {
}

void MarketData::operator()(Event<Start> const &) {
  (*connection_).start();
}

void MarketData::operator()(Event<Stop> const &) {
  (*connection_).stop();
}

void MarketData::operator()(Event<Timer> const &event) {
  auto now = event.value.now;
  (*connection_).refresh(now);
  if ((*connection_).ready()) {
    check_subscribe_queue(now);
  }
}

void MarketData::operator()(metrics::Writer &writer) const {
  writer
      // counter
      .write(counter_.disconnect, metrics::Type::COUNTER)
      // profile
      .write(profile_.parse, metrics::Type::PROFILE)
      .write(profile_.error, metrics::Type::PROFILE)
      .write(profile_.subscribe, metrics::Type::PROFILE)
      .write(profile_.unsubscribe, metrics::Type::PROFILE)
      .write(profile_.login, metrics::Type::PROFILE)
      .write(profile_.estimated_price, metrics::Type::PROFILE)
      .write(profile_.price_limit, metrics::Type::PROFILE)
      .write(profile_.mark_price, metrics::Type::PROFILE)
      .write(profile_.tickers, metrics::Type::PROFILE)
      .write(profile_.trades, metrics::Type::PROFILE)
      .write(profile_.bbo_tbt, metrics::Type::PROFILE)
      .write(profile_.books_l2_tbt, metrics::Type::PROFILE)
      .write(profile_.index_tickers, metrics::Type::PROFILE)
      .write(profile_.funding_rate, metrics::Type::PROFILE)
      // latency
      .write(latency_.ping, metrics::Type::LATENCY)
      .write(latency_.heartbeat, metrics::Type::LATENCY);
}

void MarketData::subscribe(size_t start_from) {
  if (ready()) {
    subscribe(shared_.symbols.get_slice(index_, start_from));
  }
}

// web::socket::Client::Handler

void MarketData::operator()(web::socket::Client::Connected const &) {
}

void MarketData::operator()(web::socket::Client::Disconnected const &) {
  ++counter_.disconnect;
  (*this)(ConnectionStatus::DISCONNECTED);
  download_.reset();
  subscribe_queue_.clear();
}

void MarketData::operator()(web::socket::Client::Ready const &) {
  download_.begin();
}

void MarketData::operator()(web::socket::Client::Close const &) {
}

void MarketData::operator()(web::socket::Client::Latency const &latency) {
  TraceInfo trace_info;
  auto external_latency = ExternalLatency{
      .stream_id = stream_id_,
      .account = {},
      .latency = latency.sample,
  };
  create_trace_and_dispatch(handler_, trace_info, external_latency);
  latency_.ping.update(latency.sample);
}

void MarketData::operator()(web::socket::Client::Text const &text) {
  parse(text.payload);
}

void MarketData::operator()(web::socket::Client::Binary const &) {
  log::fatal("Unexpected: binary"sv);
}

void MarketData::operator()(ConnectionStatus connection_status, std::string_view const &reason) {
  connection_status_ = connection_status;
  TraceInfo trace_info;
  auto stream_status = StreamStatus{
      .stream_id = stream_id_,
      .account = {},
      .supports = SUPPORTS,
      .transport = Transport::TCP,
      .protocol = Protocol::WS,
      .encoding = {Encoding::JSON},
      .priority = Priority::PRIMARY,
      .connection_status = connection_status_,
      .reason = reason,
      .interface = (*connection_).get_interface(),
      .authority = (*connection_).get_current_authority(),
      .path = (*connection_).get_current_path(),
      .proxy = (*connection_).get_proxy(),
  };
  log::info("stream_status={}"sv, stream_status);
  create_trace_and_dispatch(handler_, trace_info, stream_status);
}

uint32_t MarketData::download(State state) {
  switch (state) {
    using enum State;
    case UNDEFINED:
      assert(false);
      break;
    case LOGIN:
      if (std::empty(account_)) {
        log::info("Using public channels (no authentication)"sv);
        return 0;
      } else {
        (*this)(ConnectionStatus::LOGIN_SENT);
        login();
        return 1;
      }
    case DONE:
      (*this)(ConnectionStatus::READY);
      subscribe();  // note! must be *after* READY
      return 0;
  }
  assert(false);
  return 0;
}

void MarketData::login() {
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
}

void MarketData::subscribe(std::span<Symbol const> const &symbols) {
  if (std::empty(symbols)) {
    return;
  }
  // subscribe("price-limit"sv, "instType"sv, symbols);
  // subscribe("mark-price"sv, "instType"sv, symbols);
  subscribe("tickers"sv, "instId"sv, symbols);
  subscribe("trades"sv, "instId"sv, symbols);
  subscribe("bbo-tbt"sv, "instId"sv, symbols);
  auto get_books_channel = [this](auto account) {
    std::string_view result;
    auto vip = account && !shared_.settings.ws.books_use_public;
    switch (shared_.settings.ws.books_depth) {
      case 5:
        result = "books5"sv;
        break;
      case 50:
        result = "books50-l2-tbt"sv;
        if (!vip) {
          log::fatal(R"(Channel "{}" requires authentication (only available to VIP members))"sv, result);
        }
        break;
      case 400:
        result = vip ? "books-l2-tbt"sv : "books"sv;
        break;
      default:
        log::fatal("Unsupported --ws_books_depth={}"sv, shared_.settings.ws.books_depth);
    }
    log::info(R"(DEBUG: using channel="{}")"sv, result);
    return result;
  };
  subscribe(get_books_channel(!std::empty(account_)), "instId"sv, symbols);
  subscribe("index-tickers"sv, "instId"sv, symbols);
  for (auto &symbol : symbols) {
    log::info(R"(DEBUG SUBSCRIBE stream_id={}, inst_id="{}")"sv, stream_id_, static_cast<std::string_view>(symbol));
    if (shared_.settings.misc.include_bad_subscriptions ||
        shared_.extended_symbols.find(static_cast<std::string_view>(symbol)) != shared_.extended_symbols.end()) {
      // subscribe("index-tickers"sv, "instId"sv, symbol);
      subscribe("funding-rate"sv, "instId"sv, symbol);
    }
  }
}

void MarketData::subscribe(std::string_view const &channel, std::string_view const &selector, std::string_view const &value) {
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
  subscribe_queue_.emplace_back(message);
}

void MarketData::subscribe(std::string_view const &channel, std::string_view const &selector, std::span<Symbol const> const &values) {
  assert(!std::empty(values));
  auto prefix = fmt::format(
      R"({{)"
      R"("channel":"{}",)"
      R"("{}":")"sv,
      channel,
      selector);
  auto separator = fmt::format(R"("}},{})"sv, prefix);
  auto message = fmt::format(
      R"({{)"
      R"("op":"subscribe",)"
      R"("args":[)"
      R"({}{}"}})"
      R"(])"
      R"(}})"sv,
      prefix,
      fmt::join(values, separator));
  subscribe_queue_.emplace_back(message);
}

void MarketData::parse(std::string_view const &message) {
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

void MarketData::operator()(Trace<json::Error> const &event) {
  profile_.error([&]() {
    auto &[trace_info, error] = event;
    log::warn("error={}"sv, error);
  });
}

void MarketData::operator()(Trace<json::Subscribe> const &event) {
  profile_.subscribe([&]() {
    auto &[trace_info, subscribe] = event;
    log::info<1>("subscribe={}"sv, subscribe);
    if (subscribe.arg.channel == json::Channel::INSTRUMENTS && subscribe.arg.inst_type == "FUTURES"sv) {
      log::info("Request instruments..."sv);
      shared_.instruments.request = clock::get_system();
    }
  });
}

void MarketData::operator()(Trace<json::Unsubscribe> const &event) {
  profile_.unsubscribe([&]() {
    auto &[trace_info, unsubscribe] = event;
    log::info<1>("unsubscribe={}"sv, unsubscribe);
  });
}

void MarketData::operator()(Trace<json::Status> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Instruments> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::EstimatedPrice> const &event) {
  profile_.estimated_price([&]() {
    auto &[trace_info, estimated_price] = event;
    log::info<3>("estimated_price={}"sv, estimated_price);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::PriceLimit> const &event) {
  profile_.price_limit([&]() {
    auto &[trace_info, price_limit] = event;
    log::info<3>("price_limit={}"sv, price_limit);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::MarkPrice> const &event) {
  profile_.mark_price([&]() {
    auto &[trace_info, mark_price] = event;
    log::info<3>("mark_price={}"sv, mark_price);
    (*connection_).touch(trace_info.source_receive_time);
    log::fatal("HERE"sv);
  });
}

void MarketData::operator()(Trace<json::Tickers> const &event) {
  profile_.tickers([&]() {
    auto &[trace_info, tickers] = event;
    log::info<3>("tickers={}"sv, tickers);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : tickers.data) {
      std::array<Statistics, 4> statistics{{
          {
              .type = StatisticsType::OPEN_PRICE,
              .value = item.open24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::HIGHEST_TRADED_PRICE,
              .value = item.high24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::LOWEST_TRADED_PRICE,
              .value = item.low24h,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::TRADE_VOLUME,
              .value = item.vol24h,  // note! not sure...
              .begin_time_utc = {},
              .end_time_utc = {},
          },
      }};
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = item.ts,
          .exchange_sequence = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::Trades> const &event) {
  profile_.trades([&]() {
    auto &[trace_info, trades] = event;
    log::info<3>("trades={}"sv, trades);
    (*connection_).touch(trace_info.source_receive_time);
    shared_.trades.clear();
    uint64_t exchange_sequence = {};
    auto emplace_back = [&exchange_sequence](auto &result, auto &value) {
      auto trade = Trade{
          .side = map(value.side),
          .price = value.px,
          .quantity = value.sz,
          .trade_id = value.trade_id,
          .taker_order_id = {},
          .maker_order_id = {},
      };
      result.emplace_back(std::move(trade));
      exchange_sequence = std::max(exchange_sequence, value.seq_id);
    };
    std::string_view symbol;
    std::chrono::nanoseconds exchange_time_utc = {};
    for (auto &item : trades.data) {
      if (utils::update(symbol, item.inst_id) && !std::empty(shared_.trades)) {
        assert(!std::empty(symbol));
        auto trade_summary = TradeSummary{
            .stream_id = stream_id_,
            .exchange = shared_.settings.exchange,
            .symbol = symbol,
            .trades = shared_.trades,
            .exchange_time_utc = exchange_time_utc,
            .exchange_sequence = exchange_sequence,
            .sending_time_utc = {},
        };
        create_trace_and_dispatch(handler_, trace_info, trade_summary, false);
        shared_.trades.clear();
        exchange_time_utc = {};
        exchange_sequence = {};
      }
      utils::update_max(exchange_time_utc, item.ts);
      emplace_back(shared_.trades, item);
    }
    if (!std::empty(shared_.trades)) {
      assert(!std::empty(symbol));
      auto trade_summary = TradeSummary{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = symbol,
          .trades = shared_.trades,
          .exchange_time_utc = exchange_time_utc,
          .exchange_sequence = exchange_sequence,
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, trade_summary, true);
    }
  });
}

void MarketData::operator()(Trace<json::BboTbt> const &event) {
  profile_.bbo_tbt([&]() {
    auto &[trace_info, bbo_tbt] = event;
    log::info<3>("bbo_tbt={}"sv, bbo_tbt);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : bbo_tbt.data) {
      auto &bids = item.bids;
      auto &asks = item.asks;
      if (std::size(bids) > 1 || std::size(asks) > 1) {
        log::fatal("Unexpected"sv);
      }
      auto top_of_book = TopOfBook{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = bbo_tbt.arg.inst_id,
          .layer{
              .bid_price = std::empty(bids) ? NaN : bids[0].price,
              .bid_quantity = std::empty(bids) ? NaN : bids[0].size,
              .ask_price = std::empty(asks) ? NaN : asks[0].price,
              .ask_quantity = std::empty(asks) ? NaN : asks[0].size,
          },
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = item.ts,
          .exchange_sequence = item.seq_id,
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, top_of_book, true);
    }
  });
}

void MarketData::operator()(Trace<json::BooksL2Tbt> const &event) {
  profile_.books_l2_tbt([&]() {
    auto &[trace_info, books_l2_tbt] = event;
    log::info<3>(R"(books_l2_tbt={})"sv, books_l2_tbt);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : books_l2_tbt.data) {
      auto snapshot = [&]() {
        if (books_l2_tbt.action != json::Action::SNAPSHOT) {
          return false;
        }
        assert(item.prev_seq_id == -1);
        return true;
      }();
      assert(item.seq_id >= 0);
      auto &sequence = sequence_[books_l2_tbt.arg.inst_id];
      if (item.seq_id <= sequence) {
        return;
      }
      if (item.prev_seq_id >= 0 && item.prev_seq_id != sequence) {
        log::warn<1>(R"(DEBUG inst_id="{}" prev_seq_id={}, have={})"sv, books_l2_tbt.arg.inst_id, item.prev_seq_id, sequence);
      }
      sequence = item.seq_id;
      shared_.bids.clear();
      shared_.asks.clear();
      auto emplace_back = [](auto &result, auto &item) {
        auto const max_number_of_orders = std::numeric_limits<decltype(MBPUpdate::number_of_orders)>::max();
        static_assert(max_number_of_orders == 65535);
        auto number_of_orders = std::min<decltype(item.orders)>(item.orders, max_number_of_orders);
        auto mbp_update = MBPUpdate{
            .price = item.price,
            .quantity = item.size,
            .implied_quantity = NaN,
            .number_of_orders = utils::safe_cast(number_of_orders),
            .update_action = {},
            .price_level = {},
        };
        result.emplace_back(std::move(mbp_update));
      };
      for (auto &item : item.bids) {
        emplace_back(shared_.bids, item);
      }
      for (auto &item : item.asks) {
        emplace_back(shared_.asks, item);
      }
      // XXX HANS validate checksum
      auto update_type = snapshot ? UpdateType::SNAPSHOT : UpdateType::INCREMENTAL;
      auto market_by_price_update = MarketByPriceUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = books_l2_tbt.arg.inst_id,
          .bids = shared_.bids,
          .asks = shared_.asks,
          .update_type = update_type,
          .exchange_time_utc = item.ts,
          .exchange_sequence = utils::safe_cast{item.seq_id},
          .sending_time_utc = {},
          .price_precision = {},
          .quantity_precision = {},
          .checksum = {},
      };
      log::info<3>("market_by_price_update={}"sv, market_by_price_update);
      try {
        create_trace_and_dispatch(handler_, trace_info, market_by_price_update, true);
      } catch (BadState &) {
        // resubscribe_order_book_l2(symbol);
      }
    }
  });
}

void MarketData::operator()(Trace<json::IndexTickers> const &event) {
  profile_.index_tickers([&]() {
    auto &[trace_info, index_tickers] = event;
    log::info<3>("index_tickers={}"sv, index_tickers);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : index_tickers.data) {
      auto statistics = Statistics{
          .type = StatisticsType::INDEX_VALUE,
          .value = item.idx_px,
          .begin_time_utc = {},
          .end_time_utc = {},
      };
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = {&statistics, 1u},
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = {},
          .exchange_sequence = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::FundingRate> const &event) {
  profile_.funding_rate([&]() {
    auto &[trace_info, funding_rate] = event;
    log::info<3>("funding_rate={}"sv, funding_rate);
    (*connection_).touch(trace_info.source_receive_time);
    for (auto &item : funding_rate.data) {
      std::array<Statistics, 2> statistics{{
          {
              .type = StatisticsType::FUNDING_RATE,
              .value = item.funding_rate,
              .begin_time_utc = utils::safe_cast(item.funding_time),
              .end_time_utc = {},
          },
          {
              .type = StatisticsType::FUNDING_RATE_PREDICTION,
              .value = item.next_funding_rate,
              .begin_time_utc = {},
              .end_time_utc = {},
          },
      }};
      auto statistics_update = StatisticsUpdate{
          .stream_id = stream_id_,
          .exchange = shared_.settings.exchange,
          .symbol = item.inst_id,
          .statistics = statistics,
          .update_type = UpdateType::INCREMENTAL,
          .exchange_time_utc = item.ts,
          .exchange_sequence = {},
          .sending_time_utc = {},
      };
      create_trace_and_dispatch(handler_, trace_info, statistics_update, true);
    }
  });
}

void MarketData::operator()(Trace<json::ChannelConnCount> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Login> const &event) {
  profile_.login([&]() {
    auto &[trace_info, login] = event;
    log::info<1>("login={}"sv, login);
    (*connection_).touch(trace_info.source_receive_time);
    auto state = State::LOGIN;
    download_.check_relaxed(state);
  });
}

void MarketData::operator()(Trace<json::Account> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::BalanceAndPosition> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Positions> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Orders> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Order> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::AmendOrder> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::CancelOrder> const &) {
  log::fatal("Unexpected"sv);
}

void MarketData::operator()(Trace<json::Candle> const &) {
  log::fatal("Unexpected"sv);
}

// helpers

void MarketData::check_subscribe_queue(std::chrono::nanoseconds now) {
  subscribe_queue_.dispatch([&](auto now) { return shared_.rate_limiter.can_request(now); }, [&](auto &message) { (*connection_).send_text(message); }, now);
}

}  // namespace okx
}  // namespace roq
