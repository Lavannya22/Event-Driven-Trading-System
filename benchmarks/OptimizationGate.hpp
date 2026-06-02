#pragma once

#include "benchmarks/AdvancedBenchmarkRunner.hpp"
#include <string>
#include <vector>

namespace trading::bench {

// Applies the Phase 5 "keep only if ≥10% improvement" rule.
// Compares a candidate RepStats against a baseline RepStats.
struct GateResult {
    OptCategory  category;
    bool         passed{false};           // true = improvement ≥ threshold
    double       throughput_gain_pct{0};  // positive = improvement
    double       latency_gain_pct{0};     // positive = improvement (lower p99)
    std::string  verdict;
};

class OptimizationGate {
public:
    // Required improvement to retain an optimization (spec: 10%).
    static constexpr double THRESHOLD_PCT = 10.0;

    // Evaluate one optimization category against the baseline.
    static GateResult evaluate(OptCategory          category,
                               const RepStats&      baseline,
                               const RepStats&      candidate) noexcept;

    // Print a gate report to stdout.
    static void print(const GateResult& r) noexcept;

    // Run all categories and return their results.
    // Prints a summary table.
    static std::vector<GateResult> run_all(
        const RepStats& baseline,
        const std::vector<std::pair<OptCategory, RepStats>>& candidates);
};

} // namespace trading::bench
