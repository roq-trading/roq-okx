/* Copyright (c) 2017-2020, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <fmt/format.h>

#include <string>
#include <string_view>
#include <vector>

#include "roq/server.h"

namespace roq {
namespace okex {

class Config final
    : public server::Config,
      public server::ConfigReader::Handler {
 public:
  explicit Config(const std::string_view& path);

  std::string get_account() const;

  auto get_access_key() const {
    if (accounts.size() != 1)
      throw std::runtime_error("More accounts not yet supported");
    return (*accounts.begin()).second.login;
  }
  auto get_access_secret() const {
    if (accounts.size() != 1)
      throw std::runtime_error("More accounts not yet supported");
    return (*accounts.begin()).second.secret;
  }

 protected:
  // server::Config
  void dispatch(server::Config::Handler& handler) const override;

  // server::ConfigReader::Handler
  void operator()(server::Symbols&& symbols) override;
  void operator()(Account&& account) override;
  void operator()(User&& user) override;
  void operator()(
      const std::string_view& key,
      cpptoml::base& base) override;

 public:
  std::vector<User> users;
  server::Symbols symbols;
  absl::flat_hash_map<std::string, Account> accounts;
};

}  // namespace okex
}  // namespace roq

template <>
struct fmt::formatter<roq::okex::Config> {
  template <typename C>
  constexpr auto parse(C& ctx) {
    return ctx.begin();
  }
  template <typename C>
  auto format(const roq::okex::Config& value, C& ctx) {
    // FIXME(thraneh): proper
    return format_to(
        ctx.out(),
        "{{"
        "users=[{}], "
        "accounts=..."
        "}}",
        fmt::join(value.users, ", "));
  }
};
