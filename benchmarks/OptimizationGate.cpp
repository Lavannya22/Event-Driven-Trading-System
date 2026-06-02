#include "benchmarks/OptimizationGate.hpp"
#include <cstdio>

namespace trading::bench {

GateResult OptimizationGate::evaluate(OptCategory     category,
                                       const RepStats& baseline,
                                       const RepStats& candidate) noexcept {
    GateResult r;
    r.category = category;

    // Throughput: higher is better
    if (baseline.mean_throughput > 0) {
        r.throughput_gain_pct =
            (candidate.mean_throughput - baseline.mean_throughput) /
            baseline.mean_throughput * 100.0;
    }

    // Latency p99: lower is better → gain = reduction percentage
    if (baseline.mean_p99_ns > 0) {
        r.latency_gain_pct =
            (static_cast<double>(baseline.mean_p99_ns) -
             static_cast<double>(candidate.mean_p99_ns)) /
            static_cast<double>(baseline.mean_p99_ns) * 100.0;
    }

    // Pass if EITHER throughput OR latency improves by ≥ THRESHOLD_PCT
    r.passed = (r.throughput_gain_pct >= THRESHOLD_PCT ||
                r.latency_gain_pct    >= THRESHOLD_PCT);

    if (r.passed)
        r.verdict = "RETAIN — improvement >= 10%";
    else
        r.verdict = "REMOVE — improvement < 10% (rule: prove value or remove)";

    return r;
}

void OptimizationGate::print(const GateResult& r) noexcept {
    std::printf("  %-18s  throughput: %+.1f%%  p99: %+.1f%%  → %s\n",
        category_name(r.category),
        r.throughput_gain_pct,
        r.latency_gain_pct,
        r.passed ? "RETAIN ✓" : "REMOVE ✗");
}

std::vector<GateResult> OptimizationGate::run_all(
    const RepStats& baseline,
    const std::vector<std::pair<OptCategory, RepStats>>& candidates)
{
    std::vector<GateResult> results;
    results.reserve(candidates.size());

    std::printf("\n══════════════════════════════════════════════\n");
    std::printf(" Phase 5 Optimization Gate (threshold: %.0f%%)\n", THRESHOLD_PCT);
    std::printf("══════════════════════════════════════════════\n");

    for (const auto& [cat, stats] : candidates) {
        auto r = evaluate(cat, baseline, stats);
        print(r);
        results.push_back(r);
    }

    std::printf("══════════════════════════════════════════════\n\n");
    return results;
}

} // namespace trading::bench
