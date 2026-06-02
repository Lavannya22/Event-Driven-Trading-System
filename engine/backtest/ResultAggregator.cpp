#include "engine/backtest/ResultAggregator.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>

namespace trading {

void ResultAggregator::begin_run(uint32_t run_number,
                                   const StrategyConfig& cfg) noexcept {
    reset();
    run_number_ = run_number;
    config_     = cfg;
}

void ResultAggregator::on_event() noexcept {
    ++events_;
}

void ResultAggregator::on_trade(const Event& e) noexcept {
    const double price = static_cast<double>(e.price);
    const double qty   = static_cast<double>(e.quantity);
    const Side   side  = event_side(e);

    ++trades_;
    total_fill_qty_ += qty;

    // Aggressor side: Bid = buyer initiated (cost), Ask = seller initiated (revenue)
    const double value = price * qty;
    if (side == Side::Bid) {
        buy_cost_    += value;
        // A Bid aggressor "wins" if they can sell later at higher price —
        // simplified: a buy trade is counted as winning when it improves PnL.
        // For a simple model: win = buy below the running average sell price.
        // We track it as: if sell_revenue_ / buy_cost_ > 1 after this trade.
        if (sell_revenue_ > 0 && (sell_revenue_ / buy_cost_) > 1.0)
            ++winning_trades_;
    } else {
        sell_revenue_ += value;
        if (buy_cost_ > 0 && (sell_revenue_ / buy_cost_) > 1.0)
            ++winning_trades_;
    }

    // PnL and drawdown
    const double pnl = current_pnl();
    if (pnl > peak_pnl_) peak_pnl_ = pnl;
    const double drawdown = peak_pnl_ - pnl;
    if (drawdown > max_drawdown_) max_drawdown_ = drawdown;

    // Per-trade return for Sharpe (value / prior portfolio base)
    if (returns_count_ < MAX_RETURNS) {
        // Use PnL change per trade as the "return"
        returns_[returns_count_++] = pnl;
    }
}

RunResult ResultAggregator::finish_run() noexcept {
    RunResult r;
    r.run_number      = run_number_;
    r.events_processed= events_;
    r.total_trades    = trades_;
    r.win_rate        = trades_ > 0
                          ? static_cast<double>(winning_trades_) / trades_
                          : 0.0;
    r.avg_fill_size   = trades_ > 0
                          ? total_fill_qty_ / static_cast<double>(trades_)
                          : 0.0;
    r.realized_pnl    = current_pnl();
    r.sharpe_ratio    = compute_sharpe(returns_, returns_count_);
    r.max_drawdown    = max_drawdown_;
    r.config          = config_;
    return r;
}

void ResultAggregator::reset() noexcept {
    run_number_      = 0;
    events_          = 0;
    trades_          = 0;
    winning_trades_  = 0;
    total_fill_qty_  = 0;
    buy_cost_        = 0;
    sell_revenue_    = 0;
    peak_pnl_        = 0;
    max_drawdown_    = 0;
    returns_count_   = 0;
    config_          = {};
}

double ResultAggregator::compute_sharpe(const double* returns,
                                         std::size_t n) noexcept {
    // Need at least 3 PnL samples to form 2 increments for Bessel-corrected variance.
    // n=2 yields 1 increment → n-2 = 0 → division by zero.
    if (n < 3) return 0.0;

    // Compute pairwise differences (trade-to-trade PnL increments)
    double sum = 0.0;
    for (std::size_t i = 1; i < n; ++i)
        sum += returns[i] - returns[i - 1];
    const double mean = sum / static_cast<double>(n - 1);

    double var = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
        const double d = (returns[i] - returns[i - 1]) - mean;
        var += d * d;
    }
    var /= static_cast<double>(n - 2);

    const double stddev = std::sqrt(var);
    if (stddev < 1e-12) return 0.0;

    // Annualise: assume 252 trading days
    return (mean / stddev) * std::sqrt(252.0);
}

} // namespace trading
