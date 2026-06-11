include(RoqAutogen)

set(TARGET_NAME ${PROJECT_NAME}-test)

set(SOURCES
    json_account.cpp
    json_amend_order.cpp
    json_balance_and_position.cpp
    json_bbo_tbt.cpp
    json_books_l2_tbt.cpp
    json_cancel_order.cpp
    json_candle.cpp
    json_candles_ack.cpp
    json_channel_conn_count.cpp
    json_encoder.cpp
    json_error.cpp
    json_estimated_price.cpp
    json_funding_rate.cpp
    json_index_tickers.cpp
    json_instruments_ack.cpp
    json_instruments.cpp
    json_mark_price.cpp
    json_order.cpp
    json_orders.cpp
    json_orders_pending_ack.cpp
    json_positions.cpp
    json_price_limit.cpp
    json_status.cpp
    json_subscribe.cpp
    json_tickers.cpp
    json_trades.cpp
    main.cpp)

roq_gitignore(OUTPUT .gitignore SOURCES ${TARGET_NAME})

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME} PRIVATE ${PROJECT_NAME}-protocol-json roq-server::roq-server Catch2::Catch2)

if(ROQ_BUILD_TYPE STREQUAL "Release")
  set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
endif()

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
