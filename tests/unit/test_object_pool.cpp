#include "engine/memory/ObjectPool.hpp"
#include "engine/events/Event.hpp"
#include <gtest/gtest.h>

using namespace trading;

// ── helpers ──────────────────────────────────────────────────────────────────

struct Slot {
    uint64_t value{0};
    uint32_t tag{0};
};

// ── Allocation ────────────────────────────────────────────────────────────────

TEST(ObjectPool, AllocReturnsNonNullWhenAvailable) {
    ObjectPool<Slot> pool(4);
    EXPECT_NE(pool.allocate(), nullptr);
}

TEST(ObjectPool, AllocReturnsDifferentPointersEachTime) {
    ObjectPool<Slot> pool(4);
    auto* a = pool.allocate();
    auto* b = pool.allocate();
    auto* c = pool.allocate();
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

TEST(ObjectPool, AllocReturnsNullWhenExhausted) {
    ObjectPool<Slot> pool(2);
    pool.allocate();
    pool.allocate();
    EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(ObjectPool, ExhaustionCountIncrements) {
    ObjectPool<Slot> pool(1);
    pool.allocate();
    EXPECT_EQ(pool.exhaustion_count(), 0u);
    pool.allocate(); // exhausted
    EXPECT_EQ(pool.exhaustion_count(), 1u);
    pool.allocate(); // exhausted again
    EXPECT_EQ(pool.exhaustion_count(), 2u);
}

TEST(ObjectPool, ExhaustionCountZeroWhenNeverExhausted) {
    ObjectPool<Slot> pool(8);
    pool.allocate();
    pool.allocate();
    EXPECT_EQ(pool.exhaustion_count(), 0u);
}

// ── Release ───────────────────────────────────────────────────────────────────

TEST(ObjectPool, ReleaseRestoresAvailability) {
    ObjectPool<Slot> pool(1);
    auto* s = pool.allocate();
    EXPECT_EQ(pool.available(), 0u);
    pool.release(s);
    EXPECT_EQ(pool.available(), 1u);
}

TEST(ObjectPool, ReallocAfterRelease) {
    ObjectPool<Slot> pool(1);
    auto* first = pool.allocate();
    pool.release(first);
    auto* second = pool.allocate();
    EXPECT_EQ(first, second);
}

TEST(ObjectPool, ReleasedSlotIsReusable) {
    ObjectPool<Slot> pool(1);
    auto* s = pool.allocate();
    s->value = 42;
    pool.release(s);

    auto* r = pool.allocate();
    r->value = 99;
    EXPECT_EQ(r->value, 99u);
}

TEST(ObjectPool, MultipleReleasesAllowMultipleAllocs) {
    ObjectPool<Slot> pool(2);
    auto* a = pool.allocate();
    auto* b = pool.allocate();
    EXPECT_EQ(pool.allocate(), nullptr); // exhausted

    pool.release(a);
    pool.release(b);
    EXPECT_EQ(pool.available(), 2u);

    EXPECT_NE(pool.allocate(), nullptr);
    EXPECT_NE(pool.allocate(), nullptr);
    EXPECT_EQ(pool.allocate(), nullptr); // exhausted again
}

// ── Capacity / counters ───────────────────────────────────────────────────────

TEST(ObjectPool, CapacityReflectsConstructorArg) {
    ObjectPool<Slot> pool(16);
    EXPECT_EQ(pool.capacity(), 16u);
}

TEST(ObjectPool, AvailableStartsAtCapacity) {
    ObjectPool<Slot> pool(8);
    EXPECT_EQ(pool.available(), 8u);
}

TEST(ObjectPool, AvailableDecreasesOnAlloc) {
    ObjectPool<Slot> pool(4);
    EXPECT_EQ(pool.available(), 4u);
    pool.allocate();
    EXPECT_EQ(pool.available(), 3u);
    pool.allocate();
    EXPECT_EQ(pool.available(), 2u);
}

TEST(ObjectPool, UsedIncreasesOnAlloc) {
    ObjectPool<Slot> pool(4);
    EXPECT_EQ(pool.used(), 0u);
    pool.allocate();
    EXPECT_EQ(pool.used(), 1u);
    pool.allocate();
    EXPECT_EQ(pool.used(), 2u);
}

TEST(ObjectPool, UsedPlusAvailableEqualsCapacity) {
    ObjectPool<Slot> pool(8);
    pool.allocate();
    pool.allocate();
    pool.allocate();
    EXPECT_EQ(pool.used() + pool.available(), pool.capacity());
}

// ── Works with engine Event type ─────────────────────────────────────────────

TEST(ObjectPool, WorksWithEventType) {
    ObjectPool<Event> pool(32);
    EXPECT_EQ(pool.capacity(), 32u);

    auto* e = pool.allocate();
    ASSERT_NE(e, nullptr);
    e->timestamp = 123456789ULL;
    e->symbol_id = 1;
    e->type      = encode_type(EventType::NewOrder, Side::Bid);
    e->price     = 10050;
    e->quantity  = 100;
    e->order_id  = 42;

    EXPECT_EQ(event_type(*e), EventType::NewOrder);
    EXPECT_EQ(event_side(*e), Side::Bid);

    pool.release(e);
    EXPECT_EQ(pool.available(), 32u);
}

// ── Zero-capacity edge case ───────────────────────────────────────────────────

TEST(ObjectPool, ZeroCapacityAlwaysExhausted) {
    ObjectPool<Slot> pool(0);
    EXPECT_EQ(pool.capacity(), 0u);
    EXPECT_EQ(pool.available(), 0u);
    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.exhaustion_count(), 1u);
}
