add_executable(unit_tests
    test_event.cpp
    test_spsc_queue.cpp
    test_orderbook.cpp
    test_matching_engine.cpp
    test_strategy_engine.cpp
    test_replay_engine.cpp
    test_object_pool.cpp
    test_startup_allocator.cpp
    test_pool_types.cpp
    test_runtime.cpp
    test_benchmark.cpp
    test_backtest_controller.cpp
    test_result_aggregator.cpp
    test_stability_monitor.cpp
    test_overload.cpp
    test_phase5_hardware.cpp
)

target_compile_definitions(unit_tests PRIVATE
    TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(unit_tests
    PRIVATE engine
    PRIVATE benchmark_lib
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

# Allocation guard tests get their own binary because they redefine operator new.
# Mixing them into unit_tests would intercept GoogleTest's own allocations.
# AllocationGuard.cpp is compiled directly (not linked as a static lib) so the
# linker includes the operator new replacements — static libs only pull in
# object files that resolve explicit references, not replacement functions.
add_executable(alloc_guard_tests
    test_alloc_guard.cpp
    ${CMAKE_SOURCE_DIR}/engine/memory/AllocationGuard.cpp
)
# -O0: guard tests allocate + immediately delete intentionally to verify the
# override fires. At -O2 the compiler legally elides new+delete pairs whose
# result is never read, so the counter stays 0. The guard itself is always
# compiled at full optimization via the engine library.
target_compile_options(alloc_guard_tests PRIVATE -O0)
target_link_libraries(alloc_guard_tests
    PRIVATE engine
    PRIVATE GTest::gtest_main
)
gtest_discover_tests(alloc_guard_tests)
