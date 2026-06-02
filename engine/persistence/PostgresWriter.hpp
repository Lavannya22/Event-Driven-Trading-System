#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "engine/backtest/ResultAggregator.hpp"
#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"

namespace trading {

// Async PostgreSQL writer.
//
// All writes are pushed into a bounded internal queue; a background thread
// drains that queue and commits via libpq.  The hot path is NEVER blocked.
//
// Phase 4: persistence queue is bounded (MAX_QUEUE_DEPTH). When full the
// record is silently dropped and overflow_count() is incremented — the engine
// never blocks, allocates, or crashes.
//
// Build with -DWITH_POSTGRES=1 (set by CMake when libpq is found).
// Without PostgreSQL every method is a no-op.
class PostgresWriter {
public:
    static constexpr std::size_t MAX_QUEUE_DEPTH = 65536;

    struct Config {
        std::string conn_str;
        Config() : conn_str("postgresql://trading:trading@localhost:5432/trading") {}
    };

    explicit PostgresWriter(Config cfg = {});
    ~PostgresWriter();

    bool connected() const noexcept;

    // ── Phase 1/2 API ──────────────────────────────────────────────────────

    // Record the start of a replay run in replay_runs.
    std::string begin_run(const std::string& replay_file);

    // Record the end of a replay run (sets ended_at and event_count).
    void end_run(const std::string& run_id, uint64_t event_count);

    // Non-blocking: enqueue a TradeExecution event for persistence.
    void write_trade(const Event& e, const std::string& run_id);

    // ── Phase 4 API ────────────────────────────────────────────────────────

    // Persist a StrategyConfig to strategy_configs; returns the new UUID.
    std::string write_strategy_config(const StrategyConfig& cfg);

    // Record the start of a backtest run in backtest_runs.
    std::string begin_backtest_run(const std::string& replay_file,
                                   const std::string& config_id);

    // Set status + ended_at for a backtest run.
    void end_backtest_run(const std::string& run_id,
                          const std::string& status = "completed");

    // Write aggregate per-run metrics to backtest_results.
    void write_backtest_result(const std::string& run_id,
                               const RunResult& result);

    // Write latency percentiles to latency_results.
    void write_latency_result(const std::string& run_id,
                              uint64_t p50_ns, uint64_t p99_ns,
                              uint64_t p999_ns, uint64_t max_ns,
                              uint64_t throughput_eps);

    // ── Overload monitoring ────────────────────────────────────────────────

    // Number of writes dropped because the persistence queue was full.
    uint64_t overflow_count() const noexcept;

    // ── Test helpers ───────────────────────────────────────────────────────

    // Pause/resume the writer thread (test-only — simulates a slow DB).
    void pause_writer() noexcept;
    void resume_writer() noexcept;

    // Block until every previously enqueued write has been committed.
    void flush();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace trading
