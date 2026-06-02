#include "benchmarks/AdvancedBenchmarkRunner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>

namespace trading::bench {

const char* category_name(OptCategory cat) noexcept {
    switch (cat) {
        case OptCategory::Baseline:        return "Baseline";
        case OptCategory::NUMA:            return "NUMA";
        case OptCategory::HugePages:       return "HugePages";
        case OptCategory::SIMD:            return "SIMD";
        case OptCategory::DPDK:            return "DPDK";
        case OptCategory::FullOptimization:return "FullOptimization";
    }
    return "Unknown";
}

double AdvancedBenchmarkRunner::compute_mean(
    const std::vector<double>& v) noexcept {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) /
           static_cast<double>(v.size());
}

double AdvancedBenchmarkRunner::compute_median(
    std::vector<double> v) noexcept {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2 == 0) ? (v[n/2-1] + v[n/2]) / 2.0 : v[n/2];
}

double AdvancedBenchmarkRunner::compute_stddev(
    const std::vector<double>& v, double mean) noexcept {
    if (v.size() < 2) return 0.0;
    double sum = 0.0;
    for (double x : v) { double d = x - mean; sum += d * d; }
    return std::sqrt(sum / static_cast<double>(v.size() - 1));
}

RepStats AdvancedBenchmarkRunner::run(const Config& cfg) {
    std::vector<double>   throughputs;
    std::vector<uint64_t> p99_values;
    throughputs.reserve(cfg.repetitions);
    p99_values.reserve(cfg.repetitions);

    for (std::size_t i = 0; i < cfg.repetitions; ++i) {
        auto report = runner_.run(cfg.benchmark);
        throughputs.push_back(report.throughput_eps);
        p99_values.push_back(report.p99_ns);
    }

    RepStats s;
    s.repetitions = cfg.repetitions;

    // Throughput stats (higher = better)
    s.mean_throughput   = compute_mean(throughputs);
    s.median_throughput = compute_median(throughputs);
    s.stddev_throughput = compute_stddev(throughputs, s.mean_throughput);
    s.best_throughput   = *std::max_element(throughputs.begin(), throughputs.end());
    s.worst_throughput  = *std::min_element(throughputs.begin(), throughputs.end());

    // Latency p99 stats (lower = better)
    const std::vector<double> p99d(p99_values.begin(), p99_values.end());
    s.mean_p99_ns   = static_cast<uint64_t>(compute_mean(p99d));
    s.median_p99_ns = static_cast<uint64_t>(compute_median(p99d));
    s.best_p99_ns   = *std::min_element(p99_values.begin(), p99_values.end());
    s.worst_p99_ns  = *std::max_element(p99_values.begin(), p99_values.end());

    return s;
}

void AdvancedBenchmarkRunner::print_stats(const RepStats& s,
                                           OptCategory cat) const {
    std::printf("\n══════════════════════════════════════════════\n");
    std::printf(" Advanced Benchmark: %s  (%zu reps)\n",
                category_name(cat), s.repetitions);
    std::printf("──────────────────────────────────────────────\n");
    std::printf(" Throughput (events/s)\n");
    std::printf("   Mean:   %.2fM\n", s.mean_throughput   / 1e6);
    std::printf("   Median: %.2fM\n", s.median_throughput / 1e6);
    std::printf("   StdDev: %.2fM\n", s.stddev_throughput / 1e6);
    std::printf("   Best:   %.2fM\n", s.best_throughput   / 1e6);
    std::printf("   Worst:  %.2fM\n", s.worst_throughput  / 1e6);
    std::printf("──────────────────────────────────────────────\n");
    std::printf(" p99 Latency (ns)\n");
    std::printf("   Mean:   %llu\n",  (unsigned long long)s.mean_p99_ns);
    std::printf("   Median: %llu\n",  (unsigned long long)s.median_p99_ns);
    std::printf("   Best:   %llu\n",  (unsigned long long)s.best_p99_ns);
    std::printf("   Worst:  %llu\n",  (unsigned long long)s.worst_p99_ns);
    std::printf("══════════════════════════════════════════════\n\n");
}

void AdvancedBenchmarkRunner::save_json(const RepStats& s,
                                         OptCategory cat,
                                         const std::string& path) const {
    std::ofstream f(path);
    f << "{\n"
      << "  \"category\": \""        << category_name(cat)      << "\",\n"
      << "  \"repetitions\": "       << s.repetitions            << ",\n"
      << "  \"throughput_mean\": "   << s.mean_throughput        << ",\n"
      << "  \"throughput_median\": " << s.median_throughput      << ",\n"
      << "  \"throughput_stddev\": " << s.stddev_throughput      << ",\n"
      << "  \"throughput_best\": "   << s.best_throughput        << ",\n"
      << "  \"throughput_worst\": "  << s.worst_throughput       << ",\n"
      << "  \"p99_mean_ns\": "       << s.mean_p99_ns            << ",\n"
      << "  \"p99_median_ns\": "     << s.median_p99_ns          << ",\n"
      << "  \"p99_best_ns\": "       << s.best_p99_ns            << ",\n"
      << "  \"p99_worst_ns\": "      << s.worst_p99_ns           << "\n"
      << "}\n";
}

} // namespace trading::bench
