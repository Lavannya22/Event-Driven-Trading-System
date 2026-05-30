#include "engine/memory/PoolTypes.hpp"
#include "engine/memory/StartupAllocator.hpp"
#include <gtest/gtest.h>

using namespace trading;

// ── Layout assertions ─────────────────────────────────────────────────────────

TEST(PoolTypes, OrderIs64Bytes) {
    EXPECT_EQ(sizeof(Order), 64u);
}

TEST(PoolTypes, OrderAlignedTo64) {
    EXPECT_EQ(alignof(Order), 64u);
}

TEST(PoolTypes, OrderIsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<Order>);
}

TEST(PoolTypes, ExecutionReportIs64Bytes) {
    EXPECT_EQ(sizeof(ExecutionReport), 64u);
}

TEST(PoolTypes, ExecutionReportAlignedTo64) {
    EXPECT_EQ(alignof(ExecutionReport), 64u);
}

TEST(PoolTypes, ExecutionReportIsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<ExecutionReport>);
}

TEST(PoolTypes, SignalRecordIs32Bytes) {
    EXPECT_EQ(sizeof(SignalRecord), 32u);
}

TEST(PoolTypes, SignalRecordAlignedTo32) {
    EXPECT_EQ(alignof(SignalRecord), 32u);
}

TEST(PoolTypes, SignalRecordIsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<SignalRecord>);
}

// ── Pool usability ────────────────────────────────────────────────────────────

TEST(PoolTypes, StartupAllocatorCreatesAllPools) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.event_pool_size     = 64;
    cfg.order_pool_size     = 32;
    cfg.execution_pool_size = 32;
    cfg.signal_pool_size    = 32;
    cfg.attempt_mlockall    = false;

    [[maybe_unused]] auto report = alloc.start(cfg);

    ASSERT_NE(alloc.event_pool(),  nullptr);
    ASSERT_NE(alloc.order_pool(),  nullptr);
    ASSERT_NE(alloc.exec_pool(),   nullptr);
    ASSERT_NE(alloc.signal_pool(), nullptr);

    EXPECT_EQ(alloc.event_pool()->capacity(),  64u);
    EXPECT_EQ(alloc.order_pool()->capacity(),  32u);
    EXPECT_EQ(alloc.exec_pool()->capacity(),   32u);
    EXPECT_EQ(alloc.signal_pool()->capacity(), 32u);
}

TEST(PoolTypes, OrderPoolIsUsable) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.order_pool_size  = 8;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    auto* o = alloc.order_pool()->allocate();
    ASSERT_NE(o, nullptr);
    o->order_id  = 999;
    o->price     = 10050;
    o->quantity  = 100;
    o->remaining = 100;
    EXPECT_EQ(o->order_id, 999u);
    alloc.order_pool()->release(o);
    EXPECT_EQ(alloc.order_pool()->available(), 8u);
}

TEST(PoolTypes, ExecutionReportPoolIsUsable) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.execution_pool_size = 4;
    cfg.attempt_mlockall    = false;
    alloc.start(cfg);

    auto* er = alloc.exec_pool()->allocate();
    ASSERT_NE(er, nullptr);
    er->price    = 10055;
    er->quantity = 50;
    EXPECT_EQ(er->price, 10055u);
    alloc.exec_pool()->release(er);
    EXPECT_EQ(alloc.exec_pool()->available(), 4u);
}

TEST(PoolTypes, SignalRecordPoolIsUsable) {
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.signal_pool_size = 4;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    auto* sig = alloc.signal_pool()->allocate();
    ASSERT_NE(sig, nullptr);
    sig->signal      = 1;
    sig->event_index = 42;
    EXPECT_EQ(sig->signal,      1u);
    EXPECT_EQ(sig->event_index, 42u);
    alloc.signal_pool()->release(sig);
}
