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
    # Phase 5 — hardware, SIMD, transport
    hardware/HardwareTopologyManager.cpp
    hardware/NumaAllocator.cpp
    hardware/HugePageAllocator.cpp
    hardware/SimdCapabilities.cpp
    transport/SocketTransport.cpp
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

# SIMD: enable AVX2 if the compiler supports it.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx2" COMPILER_HAS_AVX2)
if(COMPILER_HAS_AVX2)
    set_source_files_properties(hardware/SimdCapabilities.cpp
        PROPERTIES COMPILE_FLAGS "-mavx2")
    target_compile_definitions(engine PUBLIC WITH_AVX2=1)
    message(STATUS "AVX2 available — SIMD price scan enabled")
else()
    message(STATUS "AVX2 not available — scalar fallback in use")
endif()

# NUMA: optional libnuma.  Install with: sudo apt install -y libnuma-dev
find_library(NUMA_LIB numa)
if(NUMA_LIB)
    target_link_libraries(engine PUBLIC ${NUMA_LIB})
    target_compile_definitions(engine PUBLIC WITH_NUMA=1)
    message(STATUS "libnuma found — NUMA-aware allocation enabled")
else()
    message(STATUS "libnuma not found — NUMA allocation falls back to malloc (install libnuma-dev to enable)")
endif()
