/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

roq::Error guess_error([[maybe_unused]] int32_t code) {
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace okx
}  // namespace roq
