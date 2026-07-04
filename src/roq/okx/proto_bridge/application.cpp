/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/proto_bridge/application.hpp"

#include "roq/logging.hpp"

#include "roq/server/proto_bridge/controller.hpp"

#include "roq/okx/gateway/controller.hpp"

#include "roq/okx/proto_bridge/config.hpp"
#include "roq/okx/proto_bridge/settings.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace proto_bridge {

// === IMPLEMENTATION ===

int Application::main(args::Parser const &args) {
  Settings settings{args};
  Config config{settings};
  log::warn("config={}"sv, config);
  auto context = server::create_io_context(settings);
  server::proto_bridge::Controller<gateway::Controller>{settings, config, *context}.dispatch();
  return EXIT_SUCCESS;
}

}  // namespace proto_bridge
}  // namespace okx
}  // namespace roq
