/* Copyright (c) 2017-2022, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <fmt/ranges.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/logging.h"
#include "roq/server.h"

namespace roq {
namespace okx {

class Config final : public server::Config, public server::ConfigReader::Handler {
 public:
  Config(const std::string_view &config_path, const std::string_view &secrets_path);

  std::string get_master_account() const;

  std::string get_api_key(const std::string_view &account) const;
  std::string get_passphrase(const std::string_view &account) const;
  std::string get_secret(const std::string_view &account) const;

 protected:
  // server::Config
  void dispatch(server::Config::Handler &) const override;

  // server::ConfigReader::Handler
  void operator()(server::Symbols &&) override;
  void operator()(server::Account &&) override;
  void operator()(server::User &&) override;
  void operator()(server::RateLimit &&) override;
  void operator()(const std::string_view &key, toml::node &) override;

 public:
  std::vector<server::User> users;
  server::Symbols symbols;
  absl::flat_hash_map<std::string, server::Account> accounts;
  std::string master_account_;
  absl::flat_hash_map<std::string, server::RateLimit> rate_limits;
};

}  // namespace okx
}  // namespace roq

template <>
struct fmt::formatter<roq::okx::Config> {
  template <typename Context>
  constexpr auto parse(Context &ctx) {
    return std::begin(ctx);
  }
  template <typename Context>
  auto format(const roq::okx::Config &value, Context &ctx) {
    using namespace std::literals;
    return fmt::format_to(
        ctx.out(),
        R"({{)"
        R"(symbols={}, )"
        R"(accounts=[{}], )"
        R"(master_account="{}", )"
        R"(users=[{}], )"
        R"(rate_limits=[{}])"
        R"(}})"sv,
        value.symbols,
        fmt::join(value.accounts, ", "sv),
        value.master_account_,
        fmt::join(value.users, ", "sv),
        fmt::join(value.rate_limits, ", "sv));
  }
};
