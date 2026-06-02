#include "engine/stability/StabilityRunner.hpp"
#include <chrono>

namespace trading {

uint64_t StabilityRunner::now_ns() noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

StabilityRunner::Report
StabilityRunner::run(BacktestController&               controller,
                     const BacktestController::Config& bt_cfg,
                     StabilityMonitor&                 monitor,
                     MemoryMonitor&                    memory,
                     const Config&                     cfg) {

    const uint64_t max_time_ns =
        static_cast<uint64_t>(cfg.duration) * 1'000'000'000ull;
    const uint64_t mem_interval_ns =
        static_cast<uint64_t>(cfg.memory_sample_interval_s) * 1'000'000'000ull;

    const uint64_t wall_start = now_ns();

    // ── Warm-up / baseline batch ──────────────────────────────────────────
    memory.set_baseline();

    auto batch_results = controller.run_all(bt_cfg);
    uint64_t total_events = 0;
    for (const auto& r : batch_results) total_events += r.events_processed;

    // Establish throughput baseline from the first batch.
    const uint64_t t_after_warmup = now_ns();
    const double elapsed_warmup_s =
        static_cast<double>(t_after_warmup - wall_start) * 1e-9;
    const double baseline_throughput =
        elapsed_warmup_s > 0.0
            ? static_cast<double>(total_events) / elapsed_warmup_s
            : 0.0;

    monitor.record_throughput(baseline_throughput);
    monitor.set_baseline();

    uint64_t last_mem_sample_ns = t_after_warmup;

    // ── Measurement loop ──────────────────────────────────────────────────
    while (true) {
        const uint64_t now = now_ns();

        if (cfg.max_events > 0 && total_events >= cfg.max_events) break;
        if (cfg.max_events == 0 && (now - wall_start) >= max_time_ns) break;

        // Memory sampling
        if ((now - last_mem_sample_ns) >= mem_interval_ns) {
            memory.record();
            monitor.record_memory_mb(memory.current_mb());
            last_mem_sample_ns = now;
        }

        // Run one batch
        batch_results = controller.run_all(bt_cfg);
        for (const auto& r : batch_results) total_events += r.events_processed;

        // Update throughput metric
        const double elapsed_s =
            static_cast<double>(now_ns() - wall_start) * 1e-9;
        if (elapsed_s > 0.0)
            monitor.record_throughput(
                static_cast<double>(total_events) / elapsed_s);
    }

    // ── Final sample ──────────────────────────────────────────────────────
    memory.record();
    monitor.record_memory_mb(memory.current_mb());

    const double actual_s =
        static_cast<double>(now_ns() - wall_start) * 1e-9;

    // ── Build report ──────────────────────────────────────────────────────
    Report report;
    report.actual_duration_s       = actual_s;
    report.events_processed        = total_events;
    report.latency_drift_pct       = monitor.latency_drift_pct();
    report.throughput_drift_pct    = monitor.throughput_drift_pct();
    report.memory_growth_mb        = memory.growth_mb();
    report.projected_24h_growth_mb = memory.projected_24h_growth_mb();
    // Queue depth is fed externally via monitor.record_queue_depth_pct();
    // the BacktestController runs synchronously so there are no live SPSC
    // queues to measure here — callers using a threaded pipeline should
    // update the monitor before each run_all() call.
    report.peak_queue_depth_pct    = monitor.queue_depth_pct();
    report.pool_exhaustions        = monitor.pool_exhaustions();
    report.alerts                  = monitor.check();
    report.passed                  = report.alerts.empty();

    return report;
}

} // namespace trading
