set(TARGET_NAME ${PROJECT_NAME}-json)

set(SOURCES map.cpp parser.cpp utils.cpp)

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
    balance_and_position_trades_item.json
    bbo_tbt.json
    books_l2_item.json
    books_l2_tbt.json
    cancel_order_ack_item.json
    cancel_order_ack.json
    category.json
    channel.json
    channel_conn_count.json
    contract_type.json
    error.json
    estimated_price_item.json
    estimated_price.json
    event_type.json
    fills_item.json
    fills.json
    frame.json
    funding_rate_item.json
    funding_rate.json
    index_tickers_item.json
    index_tickers.json
    instruments_item.json
    instruments.json
    instruments_rest.json
    instrument_state.json
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
    positions_rest.json
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

roq_autogen_hpp(
  OUTPUT
  AUTOGEN_HEADERS
  SOURCES
  ${AUTOGEN_SCHEMAS}
  TEMPLATE_DIR
  ${TEMPLATE_DIR}
  TEMPLATE_TYPE
  "json")

add_custom_target(autogen-json-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen_cpp(
  OUTPUT
  AUTOGEN_SOURCES
  SOURCES
  ${AUTOGEN_SCHEMAS}
  TEMPLATE_DIR
  ${TEMPLATE_DIR}
  TEMPLATE_TYPE
  "json")

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME} ${AUTOGEN_HEADERS} ${AUTOGEN_SOURCES})

add_library(${TARGET_NAME} OBJECT ${SOURCES} ${AUTOGEN_SOURCES})

add_dependencies(${TARGET_NAME} autogen-json-headers)

target_link_libraries(${TARGET_NAME} PRIVATE roq-core::roq-core roq-utils::roq-utils roq-logging::roq-logging fmt::fmt)
