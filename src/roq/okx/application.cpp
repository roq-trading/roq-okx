/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/okx/application.hpp"

#include "roq/okx/config.hpp"
#include "roq/okx/gateway.hpp"
#include "roq/okx/settings.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
auto const TYPE = server::Type::ORDER_MANAGEMENT;
}

// === IMPLEMENTATION ===

int Application::main(int, char **) {
  auto settings = Settings::create(TYPE);
  Config config;
  auto context = server::create_io_context();
  server::Trading<Gateway>{settings, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okx
}  // namespace roq
