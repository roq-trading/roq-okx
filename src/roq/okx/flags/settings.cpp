/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/flags/settings.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace flags {

Settings::Settings(args::Parser const &args)
    : server::flags::Settings{args, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER, ROQ_GIT_DESCRIBE_HASH, {}}, flags::Flags{flags::Flags::create()},
      misc{flags::Misc::create()}, rest{flags::REST::create()}, ws{flags::WS::create()}, mbp{flags::MBP::create()}, request{flags::Request::create()} {
  log::info("settings={}"sv, *this);
}

}  // namespace flags
}  // namespace okx
}  // namespace roq
