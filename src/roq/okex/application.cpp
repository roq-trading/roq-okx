/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/application.h"

#include "roq/okex/config.h"
#include "roq/okex/flags.h"
#include "roq/okex/gateway.h"

using namespace std::literals;

namespace roq {
namespace okex {

int Application::main(int, char **) {
  log::info(R"(Parse config_file="{}")"sv, Flags::config_file());
  Config config(Flags::config_file(), Flags::secrets_file());
  log::info<1>("config={}"sv, config);
  log::info("Starting the gateway"sv);
  roq::server::Trading<Gateway>(ROQ_PACKAGE_NAME, ROQ_BUILD_NUMBER, {}, config).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okex
}  // namespace roq
