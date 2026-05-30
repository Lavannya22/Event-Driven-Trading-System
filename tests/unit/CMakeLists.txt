add_executable(unit_tests
    test_event.cpp
    test_spsc_queue.cpp
    test_orderbook.cpp
    test_matching_engine.cpp
    test_strategy_engine.cpp
    test_replay_engine.cpp
)

target_compile_definitions(unit_tests PRIVATE
    TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(unit_tests
    PRIVATE engine
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
