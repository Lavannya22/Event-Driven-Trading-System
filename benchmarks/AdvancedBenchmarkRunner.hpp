#pragma once

#include "benchmarks/BenchmarkRunner.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace trading::bench {

// Per-repetition statistics across N runs of the same benchmark.
struct RepStats {
    double   mean_throughput{0};
    double   median_throughput{0};
    double   stddev_throughput{0};
    double   best_throughput{0};
    double   worst_throughput{0};

    uint64_t mean_p99_ns{0};
    uint64_t median_p99_ns{0};
    uint64_t best_p99_ns{0};    // lowest = best latency
    uint64_t worst_p99_ns{0};   // highest = worst latency

    std::size_t repetitions{0};
};

// Benchmark category — maps to a specific optimization configuration.
enum class OptCategory {
    Baseline,
    NUMA,
    HugePages,
    SIMD,
    DPDK,
    FullOptimization,
};

const char* category_name(OptCategory cat) noexcept;

// Runs the benchmark N times and aggregates statistics.
// The spec requires 10 repetitions; the default is set accordingly.
class AdvancedBenchmarkRunner {
public:
    static constexpr std::size_t DEFAULT_REPS = 10;

    struct Config {
        BenchmarkConfig   benchmark;
        OptCategory       category{OptCategory::Baseline};
        std::size_t       repetitions{DEFAULT_REPS};
    };

    // Run `cfg.repetitions` times and return aggregated statistics.
    RepStats run(const Config& cfg);

    void print_stats(const RepStats& s, OptCategory cat) const;
    void save_json(const RepStats& s, OptCategory cat,
                   const std::string& path) const;

private:
    BenchmarkRunner runner_;

    static double compute_mean(const std::vector<double>& v) noexcept;
    static double compute_median(std::vector<double> v) noexcept;
    static double compute_stddev(const std::vector<double>& v,
                                  double mean) noexcept;
};

} // namespace trading::bench
