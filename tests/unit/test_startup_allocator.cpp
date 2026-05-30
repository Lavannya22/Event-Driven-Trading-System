#include "engine/memory/StartupAllocator.hpp"
#include <gtest/gtest.h>

using namespace trading;

TEST(StartupAllocator, NotStartedByDefault) {
    StartupAllocator alloc;
    EXPECT_FALSE(alloc.started());
}

TEST(StartupAllocator, PoolsNullBeforeStart) {
    StartupAllocator alloc;
    EXPECT_EQ(alloc.event_pool(), nullptr);
}

TEST(StartupAllocator, StartReturnsReport) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.event_pool_size     = 1024;
    cfg.order_pool_size     = 512;
    cfg.execution_pool_size = 512;
    cfg.signal_pool_size    = 512;
    cfg.attempt_mlockall    = false;

    auto report = alloc.start(cfg);
    EXPECT_EQ(report.event_pool_capacity,     1024u);
    EXPECT_EQ(report.order_pool_capacity,      512u);
    EXPECT_EQ(report.execution_pool_capacity,  512u);
    EXPECT_EQ(report.signal_pool_capacity,     512u);
}

TEST(StartupAllocator, StartedAfterStart) {
    StartupAllocator alloc;
    alloc.start({});
    EXPECT_TRUE(alloc.started());
}

TEST(StartupAllocator, EventPoolAvailableAfterStart) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.event_pool_size  = 256;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    ASSERT_NE(alloc.event_pool(), nullptr);
    EXPECT_EQ(alloc.event_pool()->capacity(), 256u);
    EXPECT_EQ(alloc.event_pool()->available(), 256u);
}

TEST(StartupAllocator, EventPoolIsUsable) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.event_pool_size  = 4;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    auto* pool = alloc.event_pool();
    auto* e = pool->allocate();
    ASSERT_NE(e, nullptr);
    e->timestamp = 999ULL;
    EXPECT_EQ(e->timestamp, 999ULL);
    pool->release(e);
    EXPECT_EQ(pool->available(), 4u);
}

TEST(StartupAllocator, MlockallSkippedWhenDisabled) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.attempt_mlockall = false;
    auto report = alloc.start(cfg);
    EXPECT_FALSE(report.mlockall_success);
}

TEST(StartupAllocator, DefaultConfigAllocatesLargePools) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.attempt_mlockall = false;
    auto report = alloc.start(cfg);
    EXPECT_GT(report.event_pool_capacity, 0u);
    EXPECT_GT(report.order_pool_capacity, 0u);
}
