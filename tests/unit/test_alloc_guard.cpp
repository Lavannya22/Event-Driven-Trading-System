#include "engine/memory/AllocationGuard.hpp"
#include "engine/memory/ObjectPool.hpp"
#include "engine/events/Event.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace trading;

TEST(AllocationGuard, NoAllocationsWhenGuardInactive) {
    g_engine_running.store(false);
    g_unexpected_alloc_count.store(0);
    auto* p = new int(42);
    EXPECT_EQ(g_unexpected_alloc_count.load(), 0u);
    delete p;
}

TEST(AllocationGuard, CountsAllocationsWhenEngineRunning) {
    AllocationGuard guard;
    auto* p = new int(7);
    EXPECT_GE(guard.count(), 1u);
    delete p;
}

TEST(AllocationGuard, CountResetOnConstruction) {
    // First guard: make some allocations
    {
        AllocationGuard g1;
        auto* p = new int(1);
        delete p;
        EXPECT_GE(g1.count(), 1u);
    }
    // Second guard: counter starts at zero again
    AllocationGuard g2;
    EXPECT_EQ(g2.count(), 0u);
}

TEST(AllocationGuard, EngineRunningFalseAfterDestruction) {
    {
        AllocationGuard guard;
        EXPECT_TRUE(g_engine_running.load());
    }
    EXPECT_FALSE(g_engine_running.load());
}

TEST(AllocationGuard, ObjectPoolAllocDoesNotTriggerGuard) {
    // Pool storage is pre-allocated at construction (before guard is active).
    ObjectPool<Event> pool(64);

    AllocationGuard guard;
    // allocate() and release() must not call operator new
    auto* e = pool.allocate();
    ASSERT_NE(e, nullptr);
    pool.release(e);

    EXPECT_EQ(guard.count(), 0u);
}

TEST(AllocationGuard, VectorAllocationIsDetected) {
    ObjectPool<Event> pool(8); // allocate storage before guard
    AllocationGuard guard;

    // std::vector heap allocation must be detected
    std::vector<int> v;
    v.push_back(1); // triggers malloc
    EXPECT_GE(guard.count(), 1u);
}

TEST(AllocationGuard, MultipleAllocationsAllCounted) {
    AllocationGuard guard;
    auto* a = new int(1);
    auto* b = new int(2);
    auto* c = new int(3);
    EXPECT_GE(guard.count(), 3u);
    delete a; delete b; delete c;
}

TEST(AllocationGuard, NothrowNewAlsoDetected) {
    AllocationGuard guard;
    auto* p = new (std::nothrow) int(42);
    EXPECT_GE(guard.count(), 1u);
    delete p;
}
