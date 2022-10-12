/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/application.hpp"

#include "roq/okx/config.hpp"
#include "roq/okx/gateway.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === CONSTANTS ===

namespace {
server::Settings const SETTINGS{
    .package_name = ROQ_PACKAGE_NAME,
    .build_number = ROQ_BUILD_NUMBER,
    .api = {},
    .type = server::Type::ORDER_MANAGEMENT,
};
}

// === IMPLEMENTATION ===

int Application::main(int, char **) {
  Config config;
  auto context = server::create_io_context();
  server::Trading<Gateway>{SETTINGS, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okx
}  // namespace roq
