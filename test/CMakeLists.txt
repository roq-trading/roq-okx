set(TARGET_NAME "${PROJECT_NAME}-test")

set(SOURCES
    json_account.cpp
    json_amend_order_ack.cpp
    json_balance_and_position.cpp
    json_books_l2_tbt.cpp
    json_cancel_order_ack.cpp
    json_funding_rate.cpp
    json_index_tickers.cpp
    json_instruments.cpp
    json_order_ack.cpp
    json_orders.cpp
    json_positions.cpp
    json_status.cpp
    json_tickers.cpp
    json_trades.cpp
    main.cpp)

add_executable("${TARGET_NAME}" ${SOURCES})

target_link_libraries("${TARGET_NAME}" "${PROJECT_NAME}-json" roq-logging::roq-logging gtest_main
                      ${RT_LIBRARIES})

set_target_properties("${TARGET_NAME}" PROPERTIES LINK_FLAGS_RELEASE -s)

# run

add_test(NAME "${TARGET_NAME}" COMMAND "${TARGET_NAME}")
