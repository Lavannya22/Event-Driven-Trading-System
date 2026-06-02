# ── Benchmark library (shared between runner and tests) ───────────────────────
add_library(benchmark_lib STATIC
    BenchmarkRunner.cpp
    AdvancedBenchmarkRunner.cpp
    OptimizationGate.cpp
)
target_include_directories(benchmark_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(benchmark_lib PUBLIC engine)

# ── Dataset generator ─────────────────────────────────────────────────────────
add_executable(gen_datasets datasets/gen_datasets.cpp)
target_include_directories(gen_datasets PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(gen_datasets PRIVATE engine)

# ── Benchmark runner CLI ──────────────────────────────────────────────────────
add_executable(run_benchmark main.cpp)
target_include_directories(run_benchmark PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(run_benchmark PRIVATE benchmark_lib engine)

# Output directory for reports
set(BENCHMARK_REPORTS_DIR "${CMAKE_SOURCE_DIR}/benchmarks/reports")
set(BENCHMARK_DATASETS_DIR "${CMAKE_SOURCE_DIR}/benchmarks/datasets")
set(BENCHMARK_BASELINES_DIR "${CMAKE_SOURCE_DIR}/benchmarks/baselines")

target_compile_definitions(run_benchmark PRIVATE
    BENCHMARK_DATASETS_DIR="${BENCHMARK_DATASETS_DIR}"
    BENCHMARK_BASELINES_DIR="${BENCHMARK_BASELINES_DIR}"
    BENCHMARK_REPORTS_DIR="${BENCHMARK_REPORTS_DIR}"
)

target_compile_definitions(gen_datasets PRIVATE
    BENCHMARK_DATASETS_DIR="${BENCHMARK_DATASETS_DIR}"
)
