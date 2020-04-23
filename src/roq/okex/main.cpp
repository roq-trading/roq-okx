/* Copyright (c) 2017-2020, Hans Erik Thrane */

#include "roq/okex/application.h"

namespace {
constexpr std::string_view DESCRIPTION = "Roq HitBTC Gateway";
}  // namespace

int main(int argc, char **argv) {
  return roq::okex::Application(
      argc,
      argv,
      DESCRIPTION,
      VERSION).run();
}
