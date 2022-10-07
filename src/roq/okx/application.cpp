/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/application.hpp"

#include "roq/okx/config.hpp"
#include "roq/okx/gateway.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

int Application::main(int, char **) {
  Config config;
  log::info<1>("config={}"sv, config);
  auto context = server::create_io_context();
  server::Settings settings{
      .package_name = ROQ_PACKAGE_NAME,
      .build_number = ROQ_BUILD_NUMBER,
      .api = {},
      .type = server::Type::ORDER_MANAGEMENT,
  };
  server::Trading<Gateway>(settings, config, *context).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okx
}  // namespace roq
