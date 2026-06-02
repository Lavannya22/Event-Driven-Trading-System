#pragma once

#include <cstdint>

namespace trading {

// Immutable strategy parameters for one backtest run.
// Loaded before replay begins; updated only during the RESET workflow.
// The hot path reads config via a const reference — no locking required.
struct StrategyConfig {
    uint32_t window_size     = 20;    // look-back window for signal calculation
    double   threshold       = 0.01;  // signal trigger threshold
    double   inventory_limit = 1000.0;// maximum net inventory

    // Config ID for database storage (set by BacktestController).
    uint64_t config_id = 0;
};

} // namespace trading
