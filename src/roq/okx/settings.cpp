/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/settings.hpp"

#include "roq/logging.hpp"

#include "roq/okx/flags/flags.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

Settings::Settings(server::Type type)
    : server::Settings{server::create_settings(type, ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER)},
      exchange{flags::Flags::exchange()} {
  log::debug("settings={}"sv, *this);
}

}  // namespace okx
}  // namespace roq
