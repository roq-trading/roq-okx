/* Copyright (c) 2017-2026, Hans Erik Thrane */

#pragma once

#include "roq/core/limit/request.hpp"

#include "roq/okx/gateway/shared.hpp"

namespace roq {
namespace okx {
namespace gateway {

struct Request final : public core::limit::Request::Handler {
  explicit Request(Shared &);

  // core::Request::Handler

  bool ready() const override;
  bool can_request(TraceInfo const &) override;

  // ...

  core::limit::Request account;
  core::limit::Request balance;
  core::limit::Request positions;
  core::limit::Request orders;
  core::limit::Request trades;

 private:
  Shared &shared_;
};

}  // namespace gateway
}  // namespace okx
}  // namespace roq
