set(TARGET_NAME "${PROJECT_NAME}-json")

set(SOURCES parser.cpp utils.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account.json
    accounts.json
    account_type.json
    cancel_all_orders_ack_data.json
    cancel_all_orders_ack.json
    cancel_order_ack_data.json
    cancel_order_ack.json
    create_order_ack_data.json
    create_order_ack.json
    currencies.json
    currency.json
    error.json
    event_or_table.json
    event_type.json
    fills_data_item.json
    fills_data.json
    fills.json
    instance_server.json
    instrument.json
    instruments.json
    liquidity.json
    message.json
    order_book_data_item.json
    order_book_data.json
    order_book.json
    order_operation_type.json
    orders_data_item.json
    orders_data.json
    orders.json
    order_status.json
    order_type.json
    relation_context.json
    relation_event.json
    self_trade_prevention.json
    side.json
    status_item.json
    status.json
    subject.json
    subscribe.json
    symbol.json
    symbols.json
    table.json
    time_in_force.json
    token_data.json
    token.json
    trade_type.json
    type.json
    unsubscribe.json)

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
