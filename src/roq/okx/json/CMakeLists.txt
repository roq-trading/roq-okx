set(TARGET_NAME "${PROJECT_NAME}-json")

set(SOURCES parser.cpp utils.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account_item.json
    account.json
    action.json
    amend_order_ack_item.json
    amend_order_ack.json
    arg.json
    balance_and_position_balance_item.json
    balance_and_position.json
    balance_and_position_position_item.json
    books_l2_item.json
    books_l2_tbt.json
    cancel_order_ack_item.json
    cancel_order_ack.json
    category.json
    channel.json
    contract_type.json
    error.json
    estimated_price_item.json
    estimated_price.json
    event_type.json
    frame.json
    funding_rate_item.json
    funding_rate.json
    index_tickers_item.json
    index_tickers.json
    instruments_item.json
    instruments.json
    instrument_type.json
    login.json
    margin_mode.json
    mark_price_item.json
    mark_price.json
    operation.json
    option_type.json
    order_ack_item.json
    order_ack.json
    order_flow_type.json
    orders_item.json
    orders.json
    order_state.json
    order_type.json
    position_side.json
    positions_item.json
    positions.json
    price_limit_item.json
    price_limit.json
    side.json
    state.json
    status.json
    subscribe.json
    tickers_item.json
    tickers.json
    trade_mode.json
    trades_item.json
    trades.json
    trigger_price_type.json
    unsubscribe.json)

roq_autogen(
  OUTPUT
  AUTOGEN_HEADERS
  NAMESPACE
  "roq/okx/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "hpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(autogen-json-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT
  AUTOGEN_SOURCES
  NAMESPACE
  "roq/okx/json"
  OUTPUT_TYPE
  "json"
  FILE_TYPE
  "cpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

roq_gitignore(OUTPUT ".gitignore" SOURCES "${TARGET_NAME}" ${AUTOGEN_HEADERS} ${AUTOGEN_SOURCES})

add_library("${TARGET_NAME}" STATIC ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies("${TARGET_NAME}" autogen-json-headers)

target_link_libraries("${TARGET_NAME}" roq-core::roq-core roq-logging::roq-logging fmt::fmt-header-only)
