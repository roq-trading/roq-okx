/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/application.hpp"

#include "roq/okx/flags/settings.hpp"

#include "roq/okx/gateway/config.hpp"
#include "roq/okx/gateway/controller.hpp"

using namespace std::literals;

namespace roq {
namespace okx {

// === IMPLEMENTATION ===

int Application::main(args::Parser const &args) {
  flags::Settings settings{args};
  gateway::Config config{settings};
  auto context = server::create_io_context(settings);
  server::Trading<gateway::Controller>{settings, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace okx
}  // namespace roq
