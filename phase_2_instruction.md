# Phase 2 — Deterministic Foundation & Allocation-Free Runtime

## 1. Purpose

Phase 2 transforms the Phase 1 MVP from a correct trading engine into a deterministic, allocation-free, benchmarkable system.

Phase 1 established:

- Correct order book behavior
- Correct matching engine behavior
- Replay functionality
- Dashboard visualization
- Persistence layer
- Unit and integration testing

Phase 2 focuses on removing runtime unpredictability and preparing the system for meaningful performance benchmarking in Phase 3.

---

## 2. Core Principle

### Phase Progression

```text
Phase 1: Correctness
↓
Phase 2: Determinism
↓
Phase 3: Performance Optimization
↓
Phase 4: Production Stability
```

### Success Criteria for Phase 2

- No heap allocations during replay execution
- All pools allocated during startup
- Deterministic memory ownership
- Cache-aligned core structures
- Hardened lock-free SPSC queues
- Thread affinity framework available
- Timestamp abstraction available
- Multi-symbol routing supported
- Startup validation framework implemented
- Dashboard exposes deterministic runtime metrics

Latency optimization is NOT a Phase 2 goal.

---

# 3. Components To Implement

## 3.1 ObjectPool<T>

### Purpose

Provide allocation-free runtime object creation.

### Requirements

- Fixed-capacity pool
- Startup allocation only
- No runtime malloc/free
- O(1) allocate
- O(1) release

### Required API

```cpp
template<typename T>
class ObjectPool {
public:
    T* allocate();
    void release(T* object);

    size_t capacity() const;
    size_t available() const;
};
```

### Pool Exhaustion Rules

When a pool is exhausted:

- Return `nullptr`
- Increment exhaustion counter
- Update dashboard metric

Must NOT:

- Block
- Throw exceptions
- Call malloc/new
- Wait for availability

---

## 3.2 StartupAllocator

### Purpose

Create every runtime object before event processing begins.

### Responsibilities

Allocate:

- Event pools
- Order pools
- Execution report pools
- Strategy signal pools
- Ring buffer storage
- Symbol router structures

### Startup Report

```cpp
struct StartupReport {
    size_t event_pool_capacity;
    size_t order_pool_capacity;
    size_t execution_pool_capacity;
    size_t signal_pool_capacity;

    bool mlockall_success;
};
```

After startup completes:

```text
No additional allocations permitted.
```

---

## 3.3 Runtime Allocation Verification

### Tooling

Deep Validation:

- Valgrind
- Heaptrack
- AddressSanitizer

Fast CI Validation:

Override:

```cpp
operator new
operator new[]
operator new(std::nothrow)
operator new[](std::nothrow)
```

```cpp
extern bool g_engine_running;
```

When engine is running:

- Increment allocation counter
- Trigger assertion
- Fail test

Full replay must execute with:

```text
0 unexpected allocations
```

---

## 3.4 Hardened SPSC Ring Buffers

Requirements:

- Single producer
- Single consumer
- Lock free
- Wait free
- No mutexes
- No condition variables

Required memory ordering:

```cpp
tail.load(std::memory_order_acquire);
tail.store(std::memory_order_release);

head.load(std::memory_order_acquire);
head.store(std::memory_order_release);
```

Forbidden:

```cpp
std::memory_order_seq_cst
```

Stress test target:

```text
10+ million operations
```

With:

- No corruption
- No lost messages
- No deadlocks

---

## 3.5 Cache-Aligned Structures

Required alignment:

```cpp
alignas(64)
```

Apply to:

- Event
- Order
- ExecutionReport

StrategySignal:

- 32B or 64B

### Target Sizes

| Structure | Target Size |
|------------|------------|
| Event | 64B |
| Order | 64B |
| ExecutionReport | 64B |
| StrategySignal | 32–64B |

All structures require:

```cpp
static_assert(...);
```

---

## 3.6 Deterministic Memory Ownership

Allowed:

- POD structs
- References
- Raw pointers from pools
- unique_ptr for startup-only objects

Forbidden:

```cpp
std::shared_ptr
std::weak_ptr
```

Pool objects must be returned using:

```cpp
pool.release()
```

Never:

```cpp
delete
free
default unique_ptr deleter
```

---

## 3.7 Startup Validation Framework

Validation Categories:

- Memory Pools
- Ring Buffers
- Symbol Configuration
- Thread Configuration
- Timing Configuration
- NUMA Validation

Linux:

```text
Best effort NUMA verification.
```

WSL2:

```text
Skip validation.
```

Critical failures must abort startup immediately.

---

## 3.8 ThreadAffinityManager

```cpp
class ThreadAffinityManager {
public:
    bool pinThread(std::thread&, int core_id);
};
```

Linux:

```text
Failure = abort startup.
```

WSL2:

```text
Failure = warning + continue.
```

Phase 2 establishes the framework only.

---

## 3.9 TimestampProvider

```cpp
class TimestampProvider {
public:
    virtual uint64_t getTimestamp() = 0;
};
```

Implementations:

- ChronoTimestampProvider
- RdtscpTimestampProvider

If RDTSCP unsupported:

```text
Fallback to chrono.
Log warning.
```

Validation:

- Monotonic timestamps
- Fallback behavior
- No unsupported CPU crashes

---

## 3.10 SymbolRouter

```cpp
struct SymbolConfig {
    uint32_t symbol_id;
    uint64_t base_price;
    uint32_t tick_shift;
    uint32_t price_levels;
};
```

Responsibilities:

```text
symbol
↓
order book
↓
strategy
↓
matching engine
```

Events for one symbol must never affect another symbol.

---

## 3.11 Dashboard Enhancements

### Pool Metrics

- Capacity
- Used
- Available
- Exhaustion Count

### Queue Metrics

- Queue Depth
- Enqueue Count
- Dequeue Count

### Thread Metrics

- Thread IDs
- Affinity Status

### Startup Report View

- Pool capacities
- Configuration values
- Validation results

Refresh interval:

```text
250–500 ms
```

Pool exhaustion must be visually highlighted.

---

# 4. Testing Requirements

## 4.1 Unit Tests

### ObjectPool

- Allocation
- Release
- Exhaustion
- Reuse

### StartupAllocator

- Resource allocation verification

### ThreadAffinityManager

- Success path
- Failure path

### TimestampProvider

- Monotonic timestamps
- Fallback behavior

### SymbolRouter

- Correct routing
- Symbol isolation

---

## 4.2 Integration Tests

### Test 1

Full replay correctness.

### Test 2

Allocation-Free Verification

Replay + Allocation Override + ASan

Expected:

```text
0 unexpected allocations
```

### Test 3

Pool Exhaustion Recovery

```text
Pool exhausted
↓
Reject request
↓
Metric incremented
↓
Engine continues
```

### Test 4

Multi-symbol replay validation.

---

# 5. Sanitizer Requirements

### AddressSanitizer

```bash
cmake -DSANITIZER=address ..
```

### ThreadSanitizer

```bash
cmake -DSANITIZER=thread ..
```

### UndefinedBehaviorSanitizer

```bash
cmake -DSANITIZER=undefined ..
```

All tests must pass under all builds.

---

# 6. Definition of Done

## Memory

- All pools allocated during startup
- No runtime malloc/new
- Pool exhaustion handled safely
- Zero unexpected allocations during replay

## Queues

- Lock-free SPSC queue operational
- Acquire/release ordering verified

## Cache Alignment

- Core structures aligned
- Static assertions passing

## Threading

- Thread affinity framework implemented

## Timing

- Timestamp abstraction implemented
- RDTSCP fallback functioning

## Routing

- Multi-symbol support operational

## Dashboard

- Pool metrics visible
- Queue metrics visible
- Startup report visible

## Testing

- Unit tests passing
- Integration tests passing
- Sanitizer builds passing

## Determinism

- Replay produces identical results across runs

---

# 7. Deferred To Phase 3

Out of scope:

- Prefetching
- Branch minimization
- SIMD
- perf analysis
- VTune profiling
- NUMA pinning
- Cache miss optimization
- Latency benchmarking
- Bare-metal CI performance gates

---

# Recommended Build Order

1. ObjectPool<T>
2. StartupAllocator
3. Allocation verification
4. Harden SPSC queues
5. Cache-aligned structures
6. Deterministic ownership model
7. Startup validation framework
8. ThreadAffinityManager
9. TimestampProvider
10. SymbolRouter
11. Dashboard enhancements
12. Unit tests
13. Integration tests

Phase 2 ends when the engine executes deterministically with zero runtime allocations and is ready for performance optimization in Phase 3.

---

# Phase 2 Implementation Decisions & Additions

The following were built during Phase 2 but were not explicitly specified.
They are documented here so Phase 3 does not re-derive them.

---

## Naming — `SignalRecord` instead of `StrategySignal`

The spec calls the 32B pool-managed type `StrategySignal`. This name collides with
the existing `enum class StrategySignal` in `engine/strategy/Strategy.hpp`.
The pool type was renamed to `SignalRecord` to avoid the ambiguity.

```cpp
// engine/memory/PoolTypes.hpp
struct alignas(32) SignalRecord { ... };  // pool-managed, 32B
// engine/strategy/Strategy.hpp
enum class StrategySignal : uint8_t { Signal, Noop };  // unchanged
```

---

## Zero-Allocation OrderBook Refactoring

The spec requires "0 unexpected allocations during replay". Achieving this required
two structural changes to the OrderBook that are not mentioned in the spec:

### `FlatOrderMap` — replaces `std::unordered_map`

`std::unordered_map` allocates a heap node per inserted element, making it
incompatible with zero-allocation replay. Replaced with a custom open-addressing
hash map (`engine/orderbook/FlatOrderMap.hpp`) backed by a `std::vector` pre-sized
at `OrderBook` construction time.

```
key = 0        → empty slot sentinel (order_id 0 is reserved/invalid)
key = UINT64_MAX → tombstone (erased slot for linear-probe correctness)
Capacity = max_orders × 2  (50% load factor prevents probe clustering)
```

Default capacity: `OrderBook::DEFAULT_MAX_ORDERS = 65536` orders.

### `PriceLevel::order_ids` — fixed-size array

`std::vector<uint64_t>` in `PriceLevel` allocates the first time an order is
inserted at any price level. Replaced with:

```cpp
struct PriceLevel {
    static constexpr std::size_t MAX_ORDERS = 16;
    uint64_t total_quantity{0};
    std::array<uint64_t, MAX_ORDERS> order_ids{};
    uint8_t count{0};
};
```

Capacity limit: **16 orders per price level**. Insertion beyond this limit silently
fails (returns `OrderBookResult::OrderAlreadyExists`). Sufficient for Phase 2 test
workloads; Phase 3 can increase or replace with a slab-backed structure.

---

## Zero-Allocation MatchOutput

`MatchOutput::events` was `std::vector<Event>`, which allocates on the first fill.
Replaced with a fixed inline array:

```cpp
struct MatchOutput {
    static constexpr std::size_t MAX_EVENTS = 16;
    MatchResult result{MatchResult::Ok};
    std::array<Event, MAX_EVENTS> events{};
    std::size_t event_count{0};

    void push(const Event& e) noexcept;
    bool empty() const noexcept;
    std::span<const Event> event_span() const noexcept;  // range-for compatible
};
```

`event_span()` replaces range-for over the old vector. All callers updated.
Capacity limit: **16 events per match operation** (fills + one STP cancel).

---

## Event Struct — New Fields for 64B Expansion

Phase 2 expanded `Event` from 40B to 64B. The 24 extra bytes were assigned:

```cpp
uint64_t sequence;  // global sequence number (0 = unset)
uint64_t reserved;  // reserved for Phase 3 expansion
```

Both default to 0 and are transparent to all existing Phase 1 code.

---

## Allocation Guard — Build Engineering

Two non-obvious constraints discovered during implementation:

### `operator new` replacements must be compiled directly into the test binary

Static libraries only extract object files that resolve explicit symbol references.
`operator new` replacements have no callers (they silently override the C++ runtime),
so they are silently omitted when linked from a static lib.

Fix: compile `AllocationGuard.cpp` as a source file directly in the test executable's
`CMakeLists.txt`, not as a library.

### `alloc_guard_tests` must be compiled with `-O0`

At `-O2`, Clang legally eliminates `new T; delete p;` sequences whose allocated
value is never read (dead store elimination). This makes the guard tests falsely
pass with `count = 0`.

Fix: `target_compile_options(alloc_guard_tests PRIVATE -O0)`.
The guard itself runs correctly at full optimization in production — only the
*test instrumentation allocations* need to survive the optimizer.

---

## Capacity Constants

| Constant | Value | Location |
|----------|-------|----------|
| `PriceLevel::MAX_ORDERS` | 16 | `engine/orderbook/OrderBook.hpp` |
| `MatchOutput::MAX_EVENTS` | 16 | `engine/matching/MatchingEngine.hpp` |
| `SymbolRouter::kMaxSymbols` | 256 | `engine/runtime/SymbolRouter.hpp` |
| `OrderBook::DEFAULT_MAX_ORDERS` | 65536 | `engine/orderbook/OrderBook.hpp` |
| `FlatOrderMap` load factor | 50% | `engine/orderbook/FlatOrderMap.hpp` |

---

## SPSC Queue Cache Padding

The spec requires hardened SPSC queues. The concrete addition:

```cpp
alignas(kCacheLine) std::atomic<uint64_t> head_{0};  // consumer cache line
alignas(kCacheLine) std::atomic<uint64_t> tail_{0};  // producer cache line
alignas(kCacheLine) std::array<T, Capacity> buffer_{};
```

`kCacheLine = 64`. Prevents false sharing between the producer thread (writes
`tail_`) and consumer thread (writes `head_`).

---

## Dashboard — `PoolsView` React Component

A fifth tab ("Pools") was added to the React dashboard displaying:
- **Startup report** — allocator status, mlockall result, validation pass/fail, configured pool capacities
- **Live pool utilisation** — per-pool table: capacity, used, available, exhaustion count, utilisation bar

Exhaustion count is highlighted in red when non-zero. Utilisation bar turns yellow >70%, red >90%.

The existing four tabs (Order Book, Trades, Replay, Metrics) are unchanged.
