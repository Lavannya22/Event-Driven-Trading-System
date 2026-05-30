#pragma once

#include "engine/memory/ObjectPool.hpp"
#include "engine/memory/PoolTypes.hpp"
#include "engine/events/Event.hpp"
#include <cstddef>
#include <memory>

namespace trading {

struct StartupReport {
    std::size_t event_pool_capacity{0};
    std::size_t order_pool_capacity{0};
    std::size_t execution_pool_capacity{0};
    std::size_t signal_pool_capacity{0};
    bool        mlockall_success{false};
};

// Allocates every engine runtime object before event processing begins.
// After start() returns, no additional heap allocations are permitted
// in the engine hot path.
//
// Usage:
//   StartupAllocator alloc;
//   auto report = alloc.start(config);
//   // engine_running = true — hot path begins
class StartupAllocator {
public:
    struct Config {
        std::size_t event_pool_size;
        std::size_t order_pool_size;
        std::size_t execution_pool_size;
        std::size_t signal_pool_size;
        bool        attempt_mlockall;
        Config()
            : event_pool_size(65536)
            , order_pool_size(16384)
            , execution_pool_size(16384)
            , signal_pool_size(16384)
            , attempt_mlockall(true)
        {}
    };

    // Allocates all pools and optionally calls mlockall(MCL_CURRENT|MCL_FUTURE).
    // Returns a StartupReport describing what was allocated and whether
    // mlockall succeeded. Never throws — any failure is recorded in the report.
    StartupReport start(const Config& cfg = {});

    // Pool accessors — valid after start() returns.
    ObjectPool<Event>*           event_pool()     const noexcept { return event_pool_.get(); }
    ObjectPool<Order>*           order_pool()     const noexcept { return order_pool_.get(); }
    ObjectPool<ExecutionReport>* exec_pool()      const noexcept { return exec_pool_.get(); }
    ObjectPool<SignalRecord>*  signal_pool()    const noexcept { return signal_pool_.get(); }

    bool started() const noexcept { return started_; }

private:
    std::unique_ptr<ObjectPool<Event>>           event_pool_;
    std::unique_ptr<ObjectPool<Order>>           order_pool_;
    std::unique_ptr<ObjectPool<ExecutionReport>> exec_pool_;
    std::unique_ptr<ObjectPool<SignalRecord>>  signal_pool_;
    bool started_{false};
};

} // namespace trading
