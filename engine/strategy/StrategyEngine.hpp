#pragma once

#include <memory>
#include <cstdint>
#include "engine/strategy/Strategy.hpp"

namespace trading {

// Wraps a Strategy and maintains signal/noop counters for observability.
// The active strategy can be swapped at any time (not thread-safe — swap
// only between replay batches, never mid-stream).
class StrategyEngine {
public:
    explicit StrategyEngine(std::unique_ptr<Strategy> strategy);

    // Process one event through the active strategy.
    // Returns Signal (forward to matching engine) or Noop (drop).
    StrategySignal process(const Event& event) noexcept;

    // Replace the active strategy. Not safe to call while process() is running.
    void set_strategy(std::unique_ptr<Strategy> strategy);

    const Strategy& strategy()   const noexcept { return *strategy_; }
    uint64_t        signal_count() const noexcept { return signal_count_; }
    uint64_t        noop_count()   const noexcept { return noop_count_; }

    void reset_counters() noexcept { signal_count_ = 0; noop_count_ = 0; }

private:
    std::unique_ptr<Strategy> strategy_;
    uint64_t signal_count_{0};
    uint64_t noop_count_{0};
};

} // namespace trading
