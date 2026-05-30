#include "engine/runtime/StartupValidator.hpp"
#include "engine/runtime/ThreadAffinityManager.hpp"
#include "engine/runtime/TimestampProvider.hpp"
#include "engine/runtime/SymbolRouter.hpp"
#include "engine/strategy/Strategy.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace trading;

// ── StartupValidator ──────────────────────────────────────────────────────────

TEST(StartupValidator, PassesWithValidAllocator) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    StartupValidator v;
    auto report = v.validate(alloc, cfg);
    EXPECT_TRUE(report.all_passed());
}

TEST(StartupValidator, FailsIfAllocatorNotStarted) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    StartupValidator v;
    auto report = v.validate(alloc, cfg);
    EXPECT_FALSE(report.all_passed());
}

TEST(StartupValidator, ReportHasResults) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    StartupValidator v;
    auto report = v.validate(alloc, cfg);
    EXPECT_FALSE(report.results.empty());
}

TEST(StartupValidator, TimingCheckPasses) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    StartupValidator v;
    auto report = v.validate(alloc, cfg);

    bool found_timing = false;
    for (const auto& r : report.results)
        if (r.category == "Timing") found_timing = true;
    EXPECT_TRUE(found_timing);
}

// ── ThreadAffinityManager ─────────────────────────────────────────────────────

TEST(ThreadAffinityManager, PinCurrentThreadDoesNotCrash) {
    ThreadAffinityManager mgr;
    // On WSL2 this may return false — that is the expected behaviour.
    // The test just verifies it doesn't crash or throw.
    (void)mgr.pin_current_thread(0);
    SUCCEED();
}

TEST(ThreadAffinityManager, PinThreadDoesNotCrash) {
    ThreadAffinityManager mgr;
    std::thread t([] {});
    (void)mgr.pin_thread(t, 0);
    t.join();
    SUCCEED();
}

TEST(ThreadAffinityManager, Wsl2DetectionReturnsBool) {
    // Just verify it returns without crashing.
    const bool wsl = ThreadAffinityManager::is_wsl2();
    (void)wsl;
    SUCCEED();
}

// ── TimestampProvider ─────────────────────────────────────────────────────────

TEST(TimestampProvider, ChronoIsMonotonic) {
    ChronoTimestampProvider p;
    const uint64_t t0 = p.now();
    const uint64_t t1 = p.now();
    EXPECT_GE(t1, t0);
}

TEST(TimestampProvider, ChronoNameIsSet) {
    ChronoTimestampProvider p;
    EXPECT_STREQ(p.name(), "chrono");
}

TEST(TimestampProvider, ChronoReturnsNonZero) {
    ChronoTimestampProvider p;
    EXPECT_GT(p.now(), 0u);
}

TEST(TimestampProvider, RdtscpIsMonotonic) {
    RdtscpTimestampProvider p;
    const uint64_t t0 = p.now();
    const uint64_t t1 = p.now();
    EXPECT_GE(t1, t0);
}

TEST(TimestampProvider, RdtscpReturnsNonZero) {
    RdtscpTimestampProvider p;
    EXPECT_GT(p.now(), 0u);
}

TEST(TimestampProvider, RdtscpFallbackWorksWhenUnsupported) {
    RdtscpTimestampProvider p;
    // Either using RDTSCP natively or falling back to chrono — both are valid.
    // Verify the name reflects the actual mode.
    const std::string n = p.name();
    EXPECT_TRUE(n == "rdtscp" || n == "rdtscp(fallback→chrono)");
}

// ── SymbolRouter ──────────────────────────────────────────────────────────────

TEST(SymbolRouter, RegisterSymbolSucceeds) {
    SymbolRouter router;
    SymbolConfig cfg{1, 10000, 0, 1000};
    EXPECT_TRUE(router.register_symbol(cfg, std::make_unique<PassThroughStrategy>()));
    EXPECT_EQ(router.registered_count(), 1u);
}

TEST(SymbolRouter, DuplicateRegistrationFails) {
    SymbolRouter router;
    SymbolConfig cfg{1, 10000, 0, 1000};
    EXPECT_TRUE(router.register_symbol(cfg, std::make_unique<PassThroughStrategy>()));
    EXPECT_FALSE(router.register_symbol(cfg, std::make_unique<PassThroughStrategy>()));
    EXPECT_EQ(router.registered_count(), 1u);
}

TEST(SymbolRouter, OutOfRangeSymbolIdFails) {
    SymbolRouter router;
    SymbolConfig cfg{SymbolRouter::kMaxSymbols, 10000, 0, 1000};
    EXPECT_FALSE(router.register_symbol(cfg, std::make_unique<PassThroughStrategy>()));
}

TEST(SymbolRouter, BookAccessorReturnsNonNullAfterRegister) {
    SymbolRouter router;
    SymbolConfig cfg{2, 10000, 0, 1000};
    router.register_symbol(cfg, std::make_unique<PassThroughStrategy>());
    EXPECT_NE(router.book(2), nullptr);
}

TEST(SymbolRouter, BookAccessorReturnsNullForUnregistered) {
    SymbolRouter router;
    EXPECT_EQ(router.book(42), nullptr);
}

TEST(SymbolRouter, ProcessNewOrderProducesTrade) {
    SymbolRouter router;
    SymbolConfig cfg{1, 10000, 0, 1000};
    router.register_symbol(cfg, std::make_unique<PassThroughStrategy>());

    // Resting ask
    auto ask = make_new_order(1000, 1, 101, 10050, 100, Side::Ask);
    router.process(ask);

    // Aggressive bid crosses the spread
    auto bid = make_new_order(1001, 1, 102, 10050, 50, Side::Bid);
    auto out = router.process(bid);

    ASSERT_FALSE(out.empty());
    bool found_trade = false;
    for (const auto& e : out.event_span())
        if (event_type(e) == EventType::TradeExecution) found_trade = true;
    EXPECT_TRUE(found_trade);
}

TEST(SymbolRouter, SymbolIsolation) {
    SymbolRouter router;
    router.register_symbol({1, 10000, 0, 1000}, std::make_unique<PassThroughStrategy>());
    router.register_symbol({2, 20000, 0, 1000}, std::make_unique<PassThroughStrategy>());

    // Add resting order to symbol 1
    router.process(make_new_order(1000, 1, 101, 10050, 100, Side::Ask));

    // Aggressive order on symbol 2 must not affect symbol 1's book
    auto bid2 = make_new_order(1001, 2, 201, 10050, 50, Side::Bid);
    auto out2 = router.process(bid2);

    // No trade: symbol 2 has no resting orders
    for (const auto& e : out2.event_span())
        EXPECT_NE(event_type(e), EventType::TradeExecution);

    // Symbol 1's book must still have the resting ask
    EXPECT_TRUE(router.book(1)->has_ask());
}

TEST(SymbolRouter, UnknownSymbolReturnsEmptyOutput) {
    SymbolRouter router;
    auto e = make_new_order(1000, 99, 1, 10000, 100, Side::Bid);
    auto out = router.process(e);
    EXPECT_TRUE(out.empty());
}
