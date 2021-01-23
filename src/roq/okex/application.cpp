/* Copyright (c) 2017-2021, Hans Erik Thrane */

#include "roq/okex/application.h"

#include "roq/okex/config.h"
#include "roq/okex/flags.h"
#include "roq/okex/gateway.h"

namespace roq {
namespace okex {

int Application::main(int, char **) {
  LOG(INFO)("Parse configuration");
  Config config(Flags::config_file());
  VLOG(1)("config={}", config);
  LOG(INFO)("Starting the gateway");
  roq::server::Trading<Gateway>(
      ROQ_PACKAGE_NAME, config, server::RequestIdType::SEQUENTIAL, config)
      .dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okex
}  // namespace roq
