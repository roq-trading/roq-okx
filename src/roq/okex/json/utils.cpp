/* Copyright (c) 2017-2022, Hans Erik Thrane */

#include "roq/okex/json/utils.h"

using namespace std::literals;

namespace roq {
namespace okex {
namespace json {

Error guess_error(int32_t code) {
  switch (code) {
    case 200000:
      return {};
    case 200001:
      return Error::UNKNOWN;  // Order creation for this pair suspended
    case 200002:
      return Error::UNKNOWN;  // Order cancel for this pair suspended
    case 200003:
      return Error::REQUEST_RATE_LIMIT_REACHED;  // Number of orders breached the limit
    case 200009:
      return Error::UNKNOWN;  // Please complete the KYC verification before you trade XX
    case 200004:
      return Error::INSUFFICIENT_FUNDS;  // Balance insufficient
    case 400001:
      return Error::UNKNOWN;  // Any of KC-API-KEY, KC-API-SIGN, KC-API-TIMESTAMP,
                              // KC-API-PASSPHRASE is missing in your request header
    case 400002:
      return Error::UNKNOWN;  // KC-API-TIMESTAMP Invalid
    case 400003:
      return Error::UNKNOWN;  // KC-API-KEY not exists
    case 400004:
      return Error::UNKNOWN;  // KC-API-PASSPHRASE error
    case 400005:
      return Error::UNKNOWN;  // Signature error
    case 400006:
      return Error::UNKNOWN;  // The requested ip address is not in the api whitelist
    case 400007:
      return Error::UNKNOWN;  // Access Denied
    case 404000:
      return Error::UNKNOWN;  // Url Not Found
    case 400100:
      return Error::INVALID_REQUEST_ARGS;  // Parameter Error
    case 400200:
      return Error::UNKNOWN;  // Forbidden to place an order
    case 400500:
      return Error::UNKNOWN;  // Your located country/region is currently not supported for the
                              // trading of this token
    case 400700:
      return Error::UNKNOWN;  // Transaction restricted, there's a risk problem in your account
    case 400800:
      return Error::UNKNOWN;  // Leverage order failed
    case 411100:
      return Error::UNKNOWN;  // User are frozen
    case 500000:
      return Error::UNKNOWN;  // Internal Server Error
    case 900001:
      return Error::INVALID_SYMBOL;  // symbol not exists
  }
  return Error::UNKNOWN;
}

}  // namespace json
}  // namespace okex
}  // namespace roq
