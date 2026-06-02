#include <gtest/gtest.h>
#include <cmath>

#include "engine/backtest/ResultAggregator.hpp"
#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Create a TradeExecution event with an explicit side encoded in the type field.
// ResultAggregator uses event_side() to determine buy vs sell.
static Event make_fill(uint64_t price, uint64_t qty, Side side) {
    Event e{};
    e.type     = encode_type(EventType::TradeExecution, side);
    e.price    = price;
    e.quantity = qty;
    return e;
}

// ── begin_run / finish_run lifecycle ─────────────────────────────────────────

TEST(ResultAggregator, EmptyRunProducesZeroResult) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    auto r = agg.finish_run();

    EXPECT_EQ(r.run_number,       1u);
    EXPECT_EQ(r.total_trades,     0u);
    EXPECT_DOUBLE_EQ(r.realized_pnl,  0.0);
    EXPECT_DOUBLE_EQ(r.win_rate,      0.0);
    EXPECT_DOUBLE_EQ(r.avg_fill_size, 0.0);
    EXPECT_DOUBLE_EQ(r.sharpe_ratio,  0.0);
    EXPECT_DOUBLE_EQ(r.max_drawdown,  0.0);
}

TEST(ResultAggregator, RunNumberPreserved) {
    ResultAggregator agg;
    agg.begin_run(42, {});
    auto r = agg.finish_run();
    EXPECT_EQ(r.run_number, 42u);
}

TEST(ResultAggregator, ConfigPreservedInResult) {
    StrategyConfig cfg;
    cfg.window_size     = 55;
    cfg.threshold       = 0.07;
    cfg.inventory_limit = 2500.0;

    ResultAggregator agg;
    agg.begin_run(3, cfg);
    auto r = agg.finish_run();

    EXPECT_EQ(r.config.window_size, 55u);
    EXPECT_DOUBLE_EQ(r.config.threshold,       0.07);
    EXPECT_DOUBLE_EQ(r.config.inventory_limit, 2500.0);
}

TEST(ResultAggregator, ResetBetweenRunsIsolatesState) {
    ResultAggregator agg;

    // Run 1: one trade
    agg.begin_run(1, {});
    agg.on_event();
    agg.on_trade(make_fill(10000, 100, Side::Ask));
    auto r1 = agg.finish_run();

    // Run 2: no trades
    agg.begin_run(2, {});
    auto r2 = agg.finish_run();

    EXPECT_EQ(r1.total_trades, 1u);
    EXPECT_EQ(r2.total_trades, 0u);
    EXPECT_DOUBLE_EQ(r2.realized_pnl, 0.0);
}

// ── on_event ─────────────────────────────────────────────────────────────────

TEST(ResultAggregator, EventsProcessedCounted) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_event();
    agg.on_event();
    agg.on_event();
    auto r = agg.finish_run();
    EXPECT_EQ(r.events_processed, 3u);
}

// ── PnL calculation ───────────────────────────────────────────────────────────

TEST(ResultAggregator, PnLOneSellOneBuyProfitable) {
    // Sell then buy at lower price → net profit.
    // Sell 100 @ 10100 → sell_revenue = 1,010,000
    // Buy  100 @ 10000 → buy_cost     = 1,000,000
    // PnL = 10,000
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10100, 100, Side::Ask));
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, 10'000.0);
    EXPECT_EQ(r.total_trades, 2u);
}

TEST(ResultAggregator, PnLOneBuyOneSellProfitable) {
    // Buy 100 @ 10000, sell 100 @ 10050 → PnL = 5,000
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    agg.on_trade(make_fill(10050, 100, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, 5'000.0);
}

TEST(ResultAggregator, PnLLosing) {
    // Buy @ 10100, sell @ 10000 → PnL = -10,000
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10100, 100, Side::Bid));
    agg.on_trade(make_fill(10000, 100, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, -10'000.0);
}

TEST(ResultAggregator, PnLSellOnlyNoMatchingBuy) {
    // All sells, no buys → buy_cost = 0, PnL = sell_revenue
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 50, Side::Ask));
    agg.on_trade(make_fill(10000, 50, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, 1'000'000.0);
}

TEST(ResultAggregator, PnLBuyOnlyNoMatchingSell) {
    // All buys, no sells → sell_revenue = 0, PnL = -buy_cost
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, -1'000'000.0);
}

TEST(ResultAggregator, PnLMultipleFills) {
    // Sell 50@10100 + Sell 50@10200 = 505,000 + 510,000 = 1,015,000 revenue
    // Buy  50@10000 + Buy  50@10050 = 500,000 + 502,500 = 1,002,500 cost
    // PnL = 12,500
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10100, 50, Side::Ask));
    agg.on_trade(make_fill(10200, 50, Side::Ask));
    agg.on_trade(make_fill(10000, 50, Side::Bid));
    agg.on_trade(make_fill(10050, 50, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.realized_pnl, 12'500.0);
    EXPECT_EQ(r.total_trades, 4u);
}

// ── Average fill size ─────────────────────────────────────────────────────────

TEST(ResultAggregator, AvgFillSizeOneOrder) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 75, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.avg_fill_size, 75.0);
}

TEST(ResultAggregator, AvgFillSizeMultipleOrders) {
    // 100 + 50 + 150 = 300 total over 3 fills → avg = 100
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    agg.on_trade(make_fill(10000,  50, Side::Ask));
    agg.on_trade(make_fill(10000, 150, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.avg_fill_size, 100.0);
}

// ── Win rate ──────────────────────────────────────────────────────────────────

TEST(ResultAggregator, WinRateZeroTradesIsZero) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    auto r = agg.finish_run();
    EXPECT_DOUBLE_EQ(r.win_rate, 0.0);
}

TEST(ResultAggregator, WinRateProfitableScenario) {
    // Sell first → buy cheaper → second trade (buy) observed as winning.
    // Sell 100@10100, Buy 100@10000 → sell_revenue/buy_cost = 1.01 > 1.0 → 1 winning trade
    // win_rate = 1/2 = 0.5
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10100, 100, Side::Ask));
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_EQ(r.total_trades, 2u);
    EXPECT_GT(r.win_rate, 0.0);
    EXPECT_LE(r.win_rate, 1.0);
}

TEST(ResultAggregator, WinRatePurchaseOnlyNeverWins) {
    // Only buys → sell_revenue = 0 → win condition (sell_revenue > 0) never met.
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.win_rate, 0.0);
}

TEST(ResultAggregator, WinRateInRangeZeroToOne) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    for (int i = 0; i < 10; ++i) {
        agg.on_trade(make_fill(10000 + i * 10, 100, Side::Ask));
        agg.on_trade(make_fill(10000 + i *  5, 100, Side::Bid));
    }
    auto r = agg.finish_run();

    EXPECT_GE(r.win_rate, 0.0);
    EXPECT_LE(r.win_rate, 1.0);
}

// ── Max drawdown ──────────────────────────────────────────────────────────────

TEST(ResultAggregator, MaxDrawdownZeroWhenPnLAlwaysRises) {
    // Three sells in a row (sell_revenue rises monotonically, no buys):
    // pnl grows: 0 → 100,000 → 200,000 → 300,000 — never draws down.
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.max_drawdown, 0.0);
}

TEST(ResultAggregator, MaxDrawdownCapturedAfterDip) {
    // Sell 10@10100: pnl = 101,000, peak = 101,000
    // Buy  10@10200: pnl = 101,000 - 102,000 = -1,000, peak = 101,000, drawdown = 102,000
    // Sell 10@10300: pnl = 204,000 - 102,000 = 102,000, new peak = 102,000, drawdown = 0
    // max_drawdown = 102,000
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10100, 10, Side::Ask));
    agg.on_trade(make_fill(10200, 10, Side::Bid));
    agg.on_trade(make_fill(10300, 10, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_GT(r.max_drawdown, 0.0);
    EXPECT_DOUBLE_EQ(r.max_drawdown, 102'000.0);
}

TEST(ResultAggregator, MaxDrawdownNonNegative) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 100, Side::Bid));
    agg.on_trade(make_fill(10050, 100, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_GE(r.max_drawdown, 0.0);
}

// ── Sharpe ratio ──────────────────────────────────────────────────────────────

TEST(ResultAggregator, SharpeZeroTradesIsZero) {
    ResultAggregator agg;
    agg.begin_run(1, {});
    auto r = agg.finish_run();
    EXPECT_DOUBLE_EQ(r.sharpe_ratio, 0.0);
}

TEST(ResultAggregator, SharpeOneTrade) {
    // n < 3 → Sharpe = 0
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    auto r = agg.finish_run();
    EXPECT_DOUBLE_EQ(r.sharpe_ratio, 0.0);
}

TEST(ResultAggregator, SharpeTwoTrades) {
    // n < 3 → Sharpe = 0 (Bessel-corrected variance needs ≥ 3 samples)
    ResultAggregator agg;
    agg.begin_run(1, {});
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    agg.on_trade(make_fill(10000, 10, Side::Ask));
    auto r = agg.finish_run();
    EXPECT_DOUBLE_EQ(r.sharpe_ratio, 0.0);
}

TEST(ResultAggregator, SharpeConstantReturnsIsZero) {
    // Identical PnL increments → zero variance → Sharpe = 0
    ResultAggregator agg;
    agg.begin_run(1, {});
    for (int i = 0; i < 10; ++i)
        agg.on_trade(make_fill(10000, 10, Side::Ask));  // each adds same amount
    auto r = agg.finish_run();

    EXPECT_DOUBLE_EQ(r.sharpe_ratio, 0.0);
}

TEST(ResultAggregator, SharpePositiveForRisingPnL) {
    // Sells at progressively higher prices → pnl grows, increments positive,
    // positive mean with positive variance → positive Sharpe.
    ResultAggregator agg;
    agg.begin_run(1, {});
    for (int i = 1; i <= 20; ++i)
        agg.on_trade(make_fill(static_cast<uint64_t>(10000 + i * 50), 1, Side::Ask));
    auto r = agg.finish_run();

    EXPECT_GT(r.sharpe_ratio, 0.0);
    EXPECT_TRUE(std::isfinite(r.sharpe_ratio));
}

TEST(ResultAggregator, SharpeIsFiniteForLargeTradeSet) {
    // Stress: fill the ring buffer (MAX_RETURNS = 65536) with alternating fills.
    // Sharpe must be finite and not NaN/Inf.
    ResultAggregator agg;
    agg.begin_run(1, {});
    for (int i = 0; i < 200; ++i) {
        agg.on_trade(make_fill(10000 + static_cast<uint64_t>(i % 10) * 10, 1, Side::Ask));
        agg.on_trade(make_fill(10000 + static_cast<uint64_t>(i % 5)  *  5, 1, Side::Bid));
    }
    auto r = agg.finish_run();

    EXPECT_TRUE(std::isfinite(r.sharpe_ratio))
        << "Sharpe = " << r.sharpe_ratio;
}
