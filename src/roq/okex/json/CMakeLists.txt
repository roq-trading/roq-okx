set(TARGET_NAME "${PROJECT_NAME}-json")

set(SOURCES
  utils.cpp
)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
  error.json
  method.json
  order.json
  orderbook.json
  orderbook_item.json
  orders.json
  report.json
  report_type.json
  request_type.json
  response.json
  side.json
  status.json
  symbol.json
  symbols.json
  ticker.json
  time_in_force.json
  trades.json
  trades_item.json
  trading_balance.json
  trading_balance_item.json
  type.json
)

roq_autogen(
  OUTPUT AUTOGEN_HEADERS
  NAMESPACE "roq/okex/json"
  OUTPUT_TYPE "json"
  FILE_TYPE "h"
  SOURCES ${AUTOGEN_SCHEMAS})

add_custom_target(autogen-json-headers ALL
  DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT AUTOGEN_SOURCES
  NAMESPACE "roq/okex/json"
  OUTPUT_TYPE "json"
  FILE_TYPE "cpp"
  SOURCES ${AUTOGEN_SCHEMAS})

add_library("${TARGET_NAME}" STATIC
  ${SOURCES}
  ${AUTOGEN_SOURCES})

add_dependencies("${TARGET_NAME}" autogen-json-headers)

target_link_libraries("${TARGET_NAME}"
  roq-core::roq-core
  roq-logging::roq-logging
  fmt::fmt)
