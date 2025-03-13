/* Copyright (c) 2017-2025, Hans Erik Thrane */

#include "roq/okx/application.hpp"

#include "roq/okx/config.hpp"
#include "roq/okx/gateway.hpp"
#include "roq/okx/settings.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
uint8_t const API_2 = {};
}

// === IMPLEMENTATION ===

int Application::main(args::Parser const &args) {
  Settings settings{args};
  Config config{settings};
  auto context = server::create_io_context(settings);
  server::Trading<Gateway>{settings, config, *context, API_2}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okx
}  // namespace roq
