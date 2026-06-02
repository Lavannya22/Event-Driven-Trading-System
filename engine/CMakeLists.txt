add_library(engine STATIC
    events/Event.cpp
    orderbook/OrderBook.cpp
    matching/MatchingEngine.cpp
    strategy/StrategyEngine.cpp
    replay/SequentialReplayController.cpp
    persistence/PostgresWriter.cpp
    memory/StartupAllocator.cpp
    runtime/StartupValidator.cpp
    runtime/ThreadAffinityManager.cpp
    runtime/TimestampProvider.cpp
    runtime/SymbolRouter.cpp
    backtest/ResultAggregator.cpp
    backtest/BacktestController.cpp
    stability/MemoryMonitor.cpp
    stability/StabilityMonitor.cpp
    stability/StabilityRunner.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR})

# Allocation guard — separate static library so tests can opt in to the
# operator new overrides without polluting the engine library itself.
add_library(alloc_guard STATIC memory/AllocationGuard.cpp)
target_include_directories(alloc_guard PUBLIC ${CMAKE_SOURCE_DIR})

# PostgreSQL persistence — optional; no-ops gracefully when libpq-dev is absent.
# Install on Ubuntu: sudo apt install -y libpq-dev
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
    target_link_libraries(engine PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(engine PUBLIC WITH_POSTGRES=1)
    message(STATUS "PostgreSQL found — persistence layer enabled")
else()
    message(STATUS "PostgreSQL not found — persistence layer compiled as no-ops (install libpq-dev to enable)")
endif()
