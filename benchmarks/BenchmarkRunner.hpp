#pragma once

#include "benchmarks/LatencyHistogram.hpp"
#include <cstdint>
#include <string>

namespace trading::bench {

// ── Config ────────────────────────────────────────────────────────────────────

struct BenchmarkConfig {
    std::string name{"unnamed"};
    std::string dataset_path;           // path to CSV or binary replay file
    bool        binary_dataset{false};  // true = raw Event binary format

    // Warm-up terminates when EITHER threshold is met.
    uint64_t warmup_min_events{1'000'000};
    double   warmup_min_seconds{1.0};
};

// ── Report ────────────────────────────────────────────────────────────────────

struct BenchmarkReport {
    std::string name;
    std::string dataset_path;
    std::string run_timestamp;      // ISO-8601

    // Warm-up
    uint64_t warmup_events{0};
    double   warmup_duration_s{0};

    // Measurement
    uint64_t events_processed{0};
    double   duration_s{0};
    double   throughput_eps{0};     // events per second

    // Latency (nanoseconds)
    uint64_t p50_ns{0};
    uint64_t p95_ns{0};
    uint64_t p99_ns{0};
    uint64_t p999_ns{0};
    uint64_t max_ns{0};
};

// ── Runner ────────────────────────────────────────────────────────────────────

// Runs a replay dataset through the full engine pipeline, measures throughput
// and per-event latency, and produces a BenchmarkReport.
//
// Warm-up phase: events are processed but NOT recorded in the histogram.
// This heats instruction caches, branch predictors, and data caches, removing
// cold-start artefacts from the measurement.
class BenchmarkRunner {
public:
    // Run the benchmark and return the report.
    BenchmarkReport run(const BenchmarkConfig& cfg);

    // ── Output ──────────────────────────────────────────────────────────────
    void print_report(const BenchmarkReport& r) const;
    void save_json(const BenchmarkReport& r, const std::string& path) const;
    void save_csv(const BenchmarkReport& r, const std::string& path) const;

    // ── Baseline comparison ──────────────────────────────────────────────────
    // Returns true if the report passes (no regression beyond threshold).
    // threshold: 0.10 = 10% allowed degradation.
    bool compare_baseline(const BenchmarkReport& r,
                          const std::string& baseline_json_path,
                          double threshold = 0.10) const;

private:
    LatencyHistogram histogram_;
};

} // namespace trading::bench
