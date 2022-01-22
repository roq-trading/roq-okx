/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okx/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

roq::Error guess_error(int32_t code) {
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace okx
}  // namespace roq
