#include "benchmarks/BenchmarkRunner.hpp"
#include "engine/events/Event.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include "engine/replay/SequentialReplayController.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef __linux__
#  include <time.h>   // clock_gettime / CLOCK_MONOTONIC_RAW
#endif

namespace trading::bench {

// ── Nanosecond timer ──────────────────────────────────────────────────────────

static inline uint64_t now_ns() noexcept {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ull +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(
            steady_clock::now().time_since_epoch()).count());
#endif
}

static std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// ── run() ─────────────────────────────────────────────────────────────────────

BenchmarkReport BenchmarkRunner::run(const BenchmarkConfig& cfg) {
    histogram_.reset();

    // Engine setup — one symbol, pass-through strategy.
    OrderBook      book(1);
    MatchingEngine matcher;
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());

    SequentialReplayController rc(cfg.dataset_path);

    BenchmarkReport report;
    report.name         = cfg.name;
    report.dataset_path = cfg.dataset_path;
    report.run_timestamp = iso_timestamp();

    // ── Warm-up ───────────────────────────────────────────────────────────────
    const double wu_min_s = cfg.warmup_min_seconds;
    const uint64_t wu_min_ev = cfg.warmup_min_events;

    const uint64_t wu_start = now_ns();
    uint64_t wu_count = 0;
    Event ev{};

    while (rc.next(ev)) {
        if (strategy.process(ev) == StrategySignal::Noop) continue;
        if (event_type(ev) == EventType::NewOrder)
            matcher.process_new_order(book, ev);
        else if (event_type(ev) == EventType::CancelOrder)
            matcher.process_cancel(book, ev);
        ++wu_count;

        const double elapsed_s =
            static_cast<double>(now_ns() - wu_start) * 1e-9;
        if (wu_count >= wu_min_ev && elapsed_s >= wu_min_s) break;
    }

    const uint64_t wu_end = now_ns();
    report.warmup_events     = wu_count;
    report.warmup_duration_s = static_cast<double>(wu_end - wu_start) * 1e-9;

    // Reset engine state and replay source for measurement.
    book  = OrderBook(1);
    rc.reset();

    // ── Measurement ───────────────────────────────────────────────────────────
    const uint64_t meas_start = now_ns();
    uint64_t meas_count = 0;

    while (rc.next(ev)) {
        const uint64_t t0 = now_ns();

        if (strategy.process(ev) == StrategySignal::Noop) continue;
        if (event_type(ev) == EventType::NewOrder)
            matcher.process_new_order(book, ev);
        else if (event_type(ev) == EventType::CancelOrder)
            matcher.process_cancel(book, ev);

        const uint64_t t1 = now_ns();
        histogram_.record(t1 - t0);
        ++meas_count;
    }

    const uint64_t meas_end = now_ns();
    const double dur_s = static_cast<double>(meas_end - meas_start) * 1e-9;

    report.events_processed = meas_count;
    report.duration_s       = dur_s;
    report.throughput_eps   = dur_s > 0.0 ? static_cast<double>(meas_count) / dur_s : 0.0;
    report.p50_ns  = histogram_.p50();
    report.p95_ns  = histogram_.p95();
    report.p99_ns  = histogram_.p99();
    report.p999_ns = histogram_.p999();
    report.max_ns  = histogram_.max_ns();

    return report;
}

// ── Output ────────────────────────────────────────────────────────────────────

void BenchmarkRunner::print_report(const BenchmarkReport& r) const {
    std::printf("\n══════════════════════════════════════════════\n");
    std::printf(" Benchmark: %s\n", r.name.c_str());
    std::printf(" Dataset:   %s\n", r.dataset_path.c_str());
    std::printf(" Time:      %s\n", r.run_timestamp.c_str());
    std::printf("──────────────────────────────────────────────\n");
    std::printf(" Warm-up:   %llu events in %.3f s\n",
                (unsigned long long)r.warmup_events, r.warmup_duration_s);
    std::printf("──────────────────────────────────────────────\n");
    std::printf(" Events:    %llu\n", (unsigned long long)r.events_processed);
    std::printf(" Duration:  %.3f s\n", r.duration_s);
    std::printf(" Throughput:%.2f M events/s\n", r.throughput_eps / 1e6);
    std::printf("──────────────────────────────────────────────\n");
    std::printf(" p50:       %llu ns\n",  (unsigned long long)r.p50_ns);
    std::printf(" p95:       %llu ns\n",  (unsigned long long)r.p95_ns);
    std::printf(" p99:       %llu ns\n",  (unsigned long long)r.p99_ns);
    std::printf(" p99.9:     %llu ns\n",  (unsigned long long)r.p999_ns);
    std::printf(" max:       %llu ns\n",  (unsigned long long)r.max_ns);
    std::printf("══════════════════════════════════════════════\n\n");
}

void BenchmarkRunner::save_json(const BenchmarkReport& r,
                                 const std::string& path) const {
    std::ofstream f(path);
    f << "{\n"
      << "  \"name\": \""           << r.name             << "\",\n"
      << "  \"dataset\": \""        << r.dataset_path     << "\",\n"
      << "  \"timestamp\": \""      << r.run_timestamp    << "\",\n"
      << "  \"warmup_events\": "    << r.warmup_events    << ",\n"
      << "  \"warmup_duration_s\": "<< r.warmup_duration_s<< ",\n"
      << "  \"events\": "           << r.events_processed << ",\n"
      << "  \"duration_s\": "       << r.duration_s       << ",\n"
      << "  \"throughput\": "       << r.throughput_eps   << ",\n"
      << "  \"p50\": "              << r.p50_ns           << ",\n"
      << "  \"p95\": "              << r.p95_ns           << ",\n"
      << "  \"p99\": "              << r.p99_ns           << ",\n"
      << "  \"p999\": "             << r.p999_ns          << ",\n"
      << "  \"max\": "              << r.max_ns           << "\n"
      << "}\n";
}

void BenchmarkRunner::save_csv(const BenchmarkReport& r,
                                const std::string& path) const {
    const bool write_header = !std::ifstream(path).good();
    std::ofstream f(path, std::ios::app);
    if (write_header)
        f << "name,dataset,timestamp,warmup_events,warmup_s,events,"
             "duration_s,throughput,p50,p95,p99,p999,max\n";
    f << r.name         << ","
      << r.dataset_path << ","
      << r.run_timestamp<< ","
      << r.warmup_events<< ","
      << r.warmup_duration_s << ","
      << r.events_processed  << ","
      << r.duration_s        << ","
      << r.throughput_eps    << ","
      << r.p50_ns  << ","
      << r.p95_ns  << ","
      << r.p99_ns  << ","
      << r.p999_ns << ","
      << r.max_ns  << "\n";
}

// ── Baseline comparison ───────────────────────────────────────────────────────

bool BenchmarkRunner::compare_baseline(const BenchmarkReport& r,
                                        const std::string& baseline_path,
                                        double threshold) const {
    std::ifstream f(baseline_path);
    if (!f.is_open()) {
        std::fprintf(stderr,
            "[benchmark] No baseline at %s — skipping comparison.\n",
            baseline_path.c_str());
        return true;  // no baseline = pass
    }

    // Minimal JSON parse — just look for the two critical values.
    double base_throughput = 0;
    uint64_t base_p99 = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("\"throughput\"") != std::string::npos) {
            std::sscanf(line.c_str(), " \"throughput\": %lf", &base_throughput);
        }
        if (line.find("\"p99\"") != std::string::npos) {
            std::sscanf(line.c_str(), " \"p99\": %llu",
                (unsigned long long*)&base_p99);
        }
    }

    bool passed = true;

    if (base_throughput > 0) {
        const double ratio = r.throughput_eps / base_throughput;
        if (ratio < (1.0 - threshold)) {
            std::fprintf(stderr,
                "[benchmark] REGRESSION: throughput %.2fM vs baseline %.2fM (%.1f%%)\n",
                r.throughput_eps / 1e6, base_throughput / 1e6,
                (ratio - 1.0) * 100.0);
            passed = false;
        } else {
            std::printf("[benchmark] Throughput OK: %.2fM vs baseline %.2fM (%.1f%%)\n",
                r.throughput_eps / 1e6, base_throughput / 1e6,
                (ratio - 1.0) * 100.0);
        }
    }

    if (base_p99 > 0) {
        const double ratio = static_cast<double>(r.p99_ns) /
                             static_cast<double>(base_p99);
        if (ratio > (1.0 + threshold)) {
            std::fprintf(stderr,
                "[benchmark] REGRESSION: p99 %lluns vs baseline %lluns (+%.1f%%)\n",
                (unsigned long long)r.p99_ns, (unsigned long long)base_p99,
                (ratio - 1.0) * 100.0);
            passed = false;
        } else {
            std::printf("[benchmark] p99 OK: %lluns vs baseline %lluns (%.1f%%)\n",
                (unsigned long long)r.p99_ns, (unsigned long long)base_p99,
                (ratio - 1.0) * 100.0);
        }
    }

    return passed;
}

} // namespace trading::bench
