add_executable(integration_tests
    test_pipeline.cpp
    test_phase2_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/engine/memory/AllocationGuard.cpp
)

target_compile_definitions(integration_tests PRIVATE
    TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(integration_tests
    PRIVATE engine
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(integration_tests)
