/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/application.h"

#include "roq/okex/config.h"
#include "roq/okex/gateway.h"
#include "roq/okex/options.h"

namespace roq {
namespace okex {

int Application::main(int, char **) {
  LOG(INFO)("Parse configuration");
  Config config(FLAGS_config_file);
  VLOG(1)(FMT_STRING("config={}"), config);
  LOG(INFO)("Starting the gateway");
  roq::server::Trading<Gateway>(
      PACKAGE_NAME,
      config,
      FLAGS_listen,
      server::RequestIdType::SEQUENTIAL,
      config).dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okex
}  // namespace roq
