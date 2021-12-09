/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/application.h"

using namespace std::literals;

namespace {
static const auto DESCRIPTION = "Roq KuCoin Gateway"sv;
}  // namespace

int main(int argc, char **argv) {
  return roq::okex::Application(
             argc, argv, DESCRIPTION, ROQ_BUILD_VERSION, ROQ_BUILD_TYPE, ROQ_GIT_DESCRIBE_HASH)
      .run();
}
