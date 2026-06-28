/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/compat.hpp"

#include <fmt/format.h>

#include "roq/server/flags/settings.hpp"

#include "roq/okx/flags/flags.hpp"
#include "roq/okx/flags/mbp.hpp"
#include "roq/okx/flags/misc.hpp"
#include "roq/okx/flags/request.hpp"
#include "roq/okx/flags/rest.hpp"
#include "roq/okx/flags/ws.hpp"

namespace roq {
namespace okx {
namespace flags {

struct ROQ_PUBLIC Settings : public server::flags::Settings, public flags::Flags {
  explicit Settings(args::Parser const &);

  flags::Misc misc;
  flags::REST rest;
  flags::WS ws;
  flags::MBP mbp;
  flags::Request request;
};

}  // namespace flags
}  // namespace okx
}  // namespace roq

template <>
struct fmt::formatter<roq::okx::flags::Settings> {
  constexpr auto parse(format_parse_context &context) { return std::begin(context); }
  auto format(roq::okx::flags::Settings const &value, format_context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(misc={}, )"
        R"(rest={}, )"
        R"(ws={}, )"
        R"(mbp={}, )"
        R"(request={}, )"
        R"(server={})"
        R"(}})"sv,
        value.misc,
        value.rest,
        value.ws,
        value.mbp,
        value.request,
        static_cast<roq::server::Settings const &>(value));
  }
};
