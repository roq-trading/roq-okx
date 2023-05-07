/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "roq/api.hpp"
#include "roq/server.hpp"

#include "roq/core/symbols.hpp"

#include "roq/core/limit/rate_limiter.hpp"

#include "roq/okx/settings.hpp"

namespace roq {
namespace okx {

struct Shared final {
  Shared(server::Dispatcher &, Settings const &);

  Shared(Shared &&) = default;
  Shared(Shared const &) = delete;

  auto discard_symbol(std::string_view const &name) const { return dispatcher_.discard_symbol(name); }

  template <typename... Args>
  auto update_order(Args &&...args) {
    return dispatcher_.update_order(std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto operator()(Args &&...args) {
    return dispatcher_(std::forward<Args>(args)...);
  }

 public:
  std::vector<MBPUpdate> bids, asks;
  std::vector<Trade> trades;

  // private:
  server::Dispatcher &dispatcher_;

 public:
  Settings const &settings;
  core::limit::RateLimiter rate_limiter;
  core::Symbols symbols;

  // experimental
  absl::flat_hash_set<Symbol> extended_symbols;
};

}  // namespace okx
}  // namespace roq
