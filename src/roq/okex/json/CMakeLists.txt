set(TARGET_NAME "${PROJECT_NAME}-json")

set(SOURCES parser.cpp utils.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account.json
    account_balance.json
    account_balance_data.json
    account_type.json
    accounts.json
    ack.json
    cancel_all_orders_ack.json
    cancel_all_orders_ack_data.json
    cancel_order_ack.json
    cancel_order_ack_data.json
    create_order_ack.json
    create_order_ack_data.json
    currencies.json
    currency.json
    error.json
    fills.json
    fills_data.json
    fills_data_item.json
    instance_server.json
    level2.json
    level2_data.json
    level2_data_changes.json
    level2_data_changes_item.json
    liquidity.json
    message.json
    order_book.json
    order_book_data.json
    order_book_data_item.json
    order_change.json
    order_change_data.json
    order_change_type.json
    order_operation_type.json
    order_status.json
    order_type.json
    orders.json
    orders_data.json
    orders_data_item.json
    pong.json
    relation_context.json
    relation_event.json
    self_trade_prevention.json
    side.json
    snapshot.json
    snapshot_data.json
    snapshot_data_item.json
    subject.json
    symbol.json
    symbols.json
    ticker.json
    ticker_data.json
    ticker_data.json
    time_in_force.json
    token.json
    token_data.json
    trade_type.json
    type.json
    welcome.json)

roq_autogen(
  OUTPUT
  AUTOGEN_HEADERS
  NAMESPACE
  "roq/okex/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "h"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(autogen-json-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT
  AUTOGEN_SOURCES
  NAMESPACE
  "roq/okex/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "cpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

set(GITIGNORE "${CMAKE_CURRENT_SOURCE_DIR}/.gitignore")

file(WRITE "${GITIGNORE}" "")

foreach(path ${AUTOGEN_SCHEMAS})
  get_filename_component(name ${path} NAME_WE)
  file(APPEND "${GITIGNORE}" "${name}.h\n${name}.cpp\n")
endforeach()

add_library("${TARGET_NAME}" STATIC ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies("${TARGET_NAME}" autogen-json-headers)

target_link_libraries("${TARGET_NAME}" roq-core::roq-core roq-logging::roq-logging fmt::fmt)
