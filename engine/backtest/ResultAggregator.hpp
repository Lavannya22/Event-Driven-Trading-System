#pragma once

#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace trading {

struct RunResult {
    uint32_t run_number{0};
    uint64_t events_processed{0};
    uint64_t total_trades{0};
    double   win_rate{0.0};        // profitable fills / total fills
    double   avg_fill_size{0.0};
    double   realized_pnl{0.0};    // Σ(sell revenue) - Σ(buy cost)
    double   sharpe_ratio{0.0};    // (mean_return / stddev_return) × sqrt(252)
    double   max_drawdown{0.0};    // peak-to-trough PnL drop
    StrategyConfig config;
};

// Collects per-event statistics during a replay run.
// On_trade() is called from the hot path — no allocation, no locking.
class ResultAggregator {
public:
    void begin_run(uint32_t run_number, const StrategyConfig& cfg) noexcept;
    void on_event() noexcept;
    void on_trade(const Event& execution) noexcept;
    RunResult finish_run() noexcept;
    void reset() noexcept;

private:
    uint32_t run_number_{0};
    StrategyConfig config_;

    uint64_t events_{0};
    uint64_t trades_{0};
    uint64_t winning_trades_{0};
    double   total_fill_qty_{0};

    // PnL tracking
    double   buy_cost_{0};
    double   sell_revenue_{0};
    double   peak_pnl_{0};
    double   max_drawdown_{0};

    // For Sharpe: track per-trade returns in a fixed-size ring buffer.
    static constexpr std::size_t MAX_RETURNS = 65536;
    double   returns_[MAX_RETURNS]{};
    std::size_t returns_count_{0};

    double current_pnl() const noexcept { return sell_revenue_ - buy_cost_; }

    static double compute_sharpe(const double* returns,
                                  std::size_t n) noexcept;
};

} // namespace trading
