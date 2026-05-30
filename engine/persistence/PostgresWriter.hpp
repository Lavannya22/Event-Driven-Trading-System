#pragma once

#include <memory>
#include <string>
#include "engine/events/Event.hpp"

namespace trading {

// Async PostgreSQL writer.
//
// TradeExecution events are pushed from the hot path into a thread-safe queue;
// a background writer thread drains that queue and commits to PostgreSQL via
// libpq.  The engine hot path is NEVER blocked by database I/O.
//
// Build with -DWITH_POSTGRES=1 (set automatically by CMake when libpq is found).
// Without PostgreSQL, every method is a no-op so the rest of the engine compiles
// and runs unchanged.
class PostgresWriter {
public:
    struct Config {
        std::string conn_str;
        // Explicit constructor avoids a Clang limitation with default member
        // initializers inside nested structs used as default arguments.
        Config() : conn_str("postgresql://trading:trading@localhost:5432/trading") {}
    };

    explicit PostgresWriter(Config cfg = {});
    ~PostgresWriter(); // drains queue before destroying the writer thread

    // True if the initial connection succeeded.
    bool connected() const noexcept;

    // Record the start of a replay run in replay_runs.
    // Returns the generated UUID run_id — pass it to write_trade / end_run.
    std::string begin_run(const std::string& replay_file);

    // Record the end of a replay run (sets ended_at and event_count).
    void end_run(const std::string& run_id, uint64_t event_count);

    // Non-blocking: enqueue a TradeExecution event for persistence.
    void write_trade(const Event& e, const std::string& run_id);

    // Block until every previously enqueued write has been committed.
    void flush();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace trading
