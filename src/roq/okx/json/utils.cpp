/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

roq::Error guess_error(int32_t code) {
  if (code == 0) {
    return {};
  }
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace okx
}  // namespace roq
