/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/application.h"

using namespace std::literals;

namespace {
static const auto DESCRIPTION = "Roq OKEx Gateway"sv;
}  // namespace

int main(int argc, char **argv) {
  return roq::okex::Application(
             argc,
             argv,
             {
                 .description = DESCRIPTION,
                 .package_name = ROQ_PACKAGE_NAME,
                 .build_version = ROQ_BUILD_VERSION,
                 .build_number = ROQ_BUILD_NUMBER,
                 .build_type = ROQ_BUILD_TYPE,
                 .git_hash = ROQ_GIT_DESCRIBE_HASH,
             })
      .run();
}
