set(TARGET_NAME ${PROJECT_NAME}-protocol-json)

set(SOURCES encoder.cpp map.cpp parser.cpp utils.cpp)

include(RoqAutogen)

set(AUTOGEN_SCHEMAS
    account.json
    account_arg.json
    account_data_item.json
    account_item.json
    action.json
    amend_order.json
    amend_order_data_item.json
    auto_lend_status.json
    auto_staking_status.json
    balance_and_position.json
    balance_and_position_arg.json
    balance_and_position_balance_item.json
    balance_and_position_data_item.json
    balance_and_position_position_item.json
    balance_and_position_trades_item.json
    bbo_tbt.json
    bbo_tbt_arg.json
    bbo_tbt_data_item.json
    books_l2_item.json
    books_l2_tbt.json
    books_l2_tbt_arg.json
    books_l2_tbt_data_item.json
    cancel_order.json
    cancel_order_data_item.json
    candle.json
    candle_arg.json
    candle_data_item.json
    candles_ack.json
    category.json
    channel.json
    channel_conn_count.json
    contract_type.json
    error.json
    estimated_price.json
    estimated_price_arg.json
    estimated_price_data_item.json
    event_type.json
    event_type2.json
    fills_ack.json
    fills_arg.json
    fills_item.json
    funding_rate.json
    funding_rate_arg.json
    funding_rate_data_item.json
    index_tickers.json
    index_tickers_arg.json
    index_tickers_data_item.json
    instrument_state.json
    instrument_type.json
    instruments.json
    instruments_ack.json
    instruments_arg.json
    instruments_data_item.json
    login.json
    margin_mode.json
    mark_price.json
    mark_price_arg.json
    mark_price_data_item.json
    notice.json
    operation.json
    option_type.json
    order.json
    order_data_item.json
    order_flow_type.json
    order_state.json
    order_type.json
    orders.json
    orders_arg.json
    orders_data_item.json
    orders_pending_ack.json
    orders_pending_arg.json
    position_side.json
    positions.json
    positions_ack.json
    positions_arg.json
    positions_data_item.json
    price_limit.json
    price_limit_arg.json
    price_limit_data_item.json
    side.json
    state.json
    status.json
    status_arg.json
    status_data_item.json
    subscribe.json
    subscribe_arg.json
    tickers.json
    tickers_arg.json
    tickers_data_item.json
    trade_mode.json
    trades.json
    trades_arg.json
    trades_data_item.json
    trigger_price_type.json
    unsubscribe_arg.json
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

add_custom_target(protocol-json-autogen-headers ALL DEPENDS ${AUTOGEN_HEADERS})

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

add_dependencies(${TARGET_NAME} protocol-json-autogen-headers)

target_link_libraries(${TARGET_NAME} PRIVATE roq-core::roq-core roq-codec::roq-codec roq-utils::roq-utils roq-logging::roq-logging fmt::fmt)
