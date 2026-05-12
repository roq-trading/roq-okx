/* Copyright (c) 2017-2026, Hans Erik Thrane */

#include "roq/okx/json/utils.hpp"

using namespace std::literals;

namespace roq {
namespace okx {
namespace json {

// note!
// should be change to string because we have sub-codes separated by '_', e.g. 51008_1000

roq::Error guess_error(int32_t code) {
  switch (code) {
    case 0:
      return {};
    case 51008:  // Order failed. Insufficient {param0} balance in account
      return Error::INSUFFICIENT_FUNDS;
    case 51185:  // The maximum value allowed per order is {maxOrderValue} USD
      return Error::INVALID_QUANTITY;
    case 51400:  // Order cancellation failed as the order has been filled, canceled or does not exist.
      return Error::TOO_LATE_TO_MODIFY_OR_CANCEL;
    case 52916:  // Insufficient balance in funding account
      return Error::INSUFFICIENT_FUNDS;
    case 58102:  // Rate limit reached. Please refer to API docs and throttle requests accordingly.
      return Error::REQUEST_RATE_LIMIT_REACHED;
  }
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace okx
}  // namespace roq
