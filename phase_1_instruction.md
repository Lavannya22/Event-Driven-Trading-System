# Phase 1 Instructions — MVP + Full Pipeline Integration
## Production-Grade Nanosecond Low-Latency Trading Engine

---

# 1. Purpose of Phase 1

Phase 1 exists to build a fully functional deterministic trading engine MVP.

This phase focuses on:

- correctness
- deterministic replay
- architecture validation
- component integration
- observability
- persistence
- testing

This phase does NOT focus on:

- nanosecond optimization
- cache prefetch tuning
- NUMA tuning
- DPDK
- SIMD
- extreme throughput optimization

The goal is to validate the complete architectural pipeline before low-latency hardening begins.

---

# 2. Core Engineering Principle

The engine must be built in this order:

```text
Correctness
    ↓
Integration
    ↓
Determinism
    ↓
Performance
    ↓
Latency
    ↓
Reliability
```

Most important rule:

> Do NOT optimize before deterministic correctness is proven.

Otherwise:

> you optimize bugs instead of systems.

---

# 3. Phase 1 Deliverables

Phase 1 is considered complete when the following are functional:

- fixed-size event pipeline
- SPSC ring buffer
- deterministic replay engine
- order book engine
- matching engine
- strategy engine
- async persistence layer
- React debugging dashboard
- integration tests
- sanitizer validation

The system should support:

```text
Replay
→ Pipeline
→ Strategy
→ Matching
→ Execution
→ Persistence
→ Dashboard Visualization
```

---

# 4. Phase 1 System Scope

## Included Components

### Core Engine
- Event model
- SPSC queue
- Order book
- Matching engine
- Strategy engine
- Replay engine

### Observability
- Basic metrics
- Async logging
- React dashboard

### Persistence
- PostgreSQL async trade writer

### Testing
- Unit testing
- Integration testing
- Sanitizer testing

---

# 5. Recommended Technology Stack (Phase 1)

## Language
- C++20

## Build System
- CMake

## Compiler
- Clang preferred
- GCC acceptable

## Testing
- GoogleTest
- AddressSanitizer
- ThreadSanitizer
- UBSan

## Backend Runtime
- Linux via WSL2 during development
- Bare-metal Linux later phases

## Frontend
- React
- TailwindCSS
- WebSocket updates

## WebSocket Server
- uWebSockets (preferred)

Reason:
- lightweight async API
- low overhead
- simple C++ integration
- suitable for async metrics publishing

## Database
- PostgreSQL

---

## Recommended Initial Schema

```sql
CREATE TABLE executions (
    id          BIGSERIAL PRIMARY KEY,
    timestamp   BIGINT NOT NULL,
    symbol_id   INTEGER NOT NULL,
    order_id    BIGINT NOT NULL,
    price       BIGINT NOT NULL,
    quantity    BIGINT NOT NULL,
    side        SMALLINT NOT NULL,
    run_id      UUID NOT NULL
);

CREATE TABLE replay_runs (
    run_id      UUID PRIMARY KEY,
    started_at  TIMESTAMPTZ NOT NULL,
    ended_at    TIMESTAMPTZ,
    replay_file TEXT NOT NULL,
    event_count BIGINT
);
```

The run_id field is critical for deterministic replay validation across runs.

## Logging
- spdlog (async mode only)
OR
- lightweight custom async logger

---

# 6. Directory Structure

Recommended repository structure:

```text
project-root/
│
├── engine/
│   ├── events/
│   ├── queues/
│   ├── orderbook/
│   ├── matching/
│   ├── strategy/
│   ├── replay/
│   ├── execution/
│   └── persistence/
│
├── dashboard/
│   ├── frontend/
│   └── websocket/
│
├── tests/
│   ├── unit/
│   └── integration/
│
├── benchmarks/
│
├── scripts/
│
├── docs/
│
└── CMakeLists.txt
```

---

# 7. Event Model Requirements

## Objective
All engine communication must occur through deterministic fixed-size events.

---

## Required Event Types

### NewOrder
### CancelOrder
### ModifyOrder
### MarketUpdate
### TradeExecution

---

## Event Design Rules

### MUST BE
- POD structs only
- fixed-size
- serialization-friendly
- memcpy-safe

### MUST NOT CONTAIN
- std::string
- std::vector
- dynamic memory
- virtual methods
- heap ownership

---

## Recommended Initial Event Structure

```cpp
enum class EventType : uint32_t {
    NewOrder       = 1,
    CancelOrder    = 2,
    ModifyOrder    = 3,
    MarketUpdate   = 4,
    TradeExecution = 5
};

struct Event {
    uint64_t timestamp;
    uint32_t symbol_id;
    uint32_t type;
    uint64_t price;
    uint64_t quantity;
    uint64_t order_id;
};

static_assert(sizeof(Event) == 40,
              "Event size changed unexpectedly");
```

Phase 1 does NOT yet require:
- 64B alignment
- cache optimization

Those belong to Phase 2.

---

# 8. SPSC Ring Buffer Requirements

## Objective
Build deterministic communication between engine stages.

---

## Required Features

### Must support
- enqueue
- dequeue
- bounded capacity
- deterministic ordering

### Queue Type
- single-producer
- single-consumer

---

## Required Properties

### MUST
- use power-of-two capacity
- remain bounded
- preserve FIFO ordering

### REQUIRED FROM DAY ONE
- use std::atomic for head/tail indices
- maintain thread-safe queue ownership

Example:

```cpp
std::atomic<uint64_t> head_{0};
std::atomic<uint64_t> tail_{0};
```

### SHOULD NOT YET
- aggressively optimize atomics
- implement cache padding
- micro-optimize memory ordering

Correctness first.

---

# 9. Order Book Requirements

## Objective
Maintain deterministic bid/ask state.

---

## Required Features

### Bid/Ask Tracking
- best bid
- best ask
- spread
- depth levels

### Order Operations
- insert
- modify
- cancel
- remove

---

## Data Structure Requirements

### Must use
- array-based price levels
- bounded price range

### Must avoid
- tree structures
- dynamic allocation-heavy containers

---

## Out-of-Range Price Handling

Orders with prices outside the configured bounded range MUST:

- be rejected explicitly
- emit an error event or warning
- never silently corrupt order book state

Out-of-range orders must never modify live book state.

---

## Phase 1 Constraints

Do NOT yet implement:
- advanced cache optimization
- shift-based indexing optimization
- prefetching

Those belong to Phase 3.

---

# 10. Matching Engine Requirements

## Objective
Match incoming orders using price-time priority.

---

## Required Features

### Must support
- full fills
- partial fills
- execution generation
- deterministic matching order

---

## Required Matching Rules

### Matching Priority
```text
Price priority
    ↓
Time priority
```

---

## REQUIRED: Basic Self-Trade Prevention

Phase 1 MUST include:

```text
cancel aggressor
```

This prevents replay corruption and invalid executions.

---

## Required STP Behavior

When STP triggers:

- emit a CancelOrder event for the aggressor
- forward the cancellation into the execution pipeline
- persist the cancellation event
- NEVER silently drop the order

---

## Phase 1 Constraints

Do NOT yet implement:
- advanced STP modes
- branch minimization
- prefetch tuning
- advanced matching optimization

---

# 11. Strategy Engine Requirements

## Objective
Complete the end-to-end event pipeline.

---

## Phase 1 Strategy Behavior

Initial implementation may be:

```text
INPUT → SIGNAL | NOOP
```

A simple pass-through strategy is acceptable.

---

## Phase 1 Signal Semantics

### SIGNAL
Forward event downstream to matching engine.

### NOOP
Drop event intentionally without downstream forwarding.

---

## Requirements

### MUST
- produce deterministic output
- avoid blocking
- preserve ordering

### DOES NOT YET REQUIRE
- advanced strategies
- market making
- statistical arbitrage

---

# 12. Replay Engine Requirements

## Objective
Enable deterministic simulation and debugging.

---

## Required Features

### Must support
- replay from file
- deterministic sequential playback
- replay restart

---

## IMPORTANT ARCHITECTURAL REQUIREMENT

Replay system MUST expose:

```text
ReplayController interface
```

Even if Phase 1 only implements:

```text
SequentialReplayController
```

This prevents major architectural refactors later.

---

## Initial Replay Formats

Acceptable formats:
- CSV
- binary replay files
- JSON (temporary only)

Binary format is preferred long term.

---

## Recommended CSV Replay Format

```text
timestamp,symbol_id,event_type,price,quantity,order_id
1000000,1,1,10050,100,1001
1000100,1,2,10050,100,1001
```

This format should be used for:
- integration test fixtures
- replay debugging
- deterministic validation

---

# 13. Persistence Layer Requirements

## Objective
Persist executions and replay statistics.

---

## Database
- PostgreSQL

---

## Required Persistence Features

### Persist
- executions
- fills
- replay metadata
- run statistics

---

## CRITICAL RULE

Persistence MUST remain:

```text
completely outside hot path
```

Implementation pattern:

```text
Execution Engine
    ↓
Async Persistence Queue
    ↓
Writer Thread
    ↓
PostgreSQL
```

---

# 14. Dashboard Requirements (Phase 1)

## Objective
Provide visual debugging and replay validation.

This dashboard is:

```text
an observability/debugging tool
```

NOT a production operations console yet.

---

# Frontend Requirements

## Stack
- React
- TailwindCSS
- WebSocket updates

---

## Required Views

### Order Book View
Display:
- bids
- asks
- spread
- depth

---

### Trade View
Display:
- recent executions
- fills
- timestamps

---

### Replay View
Display:
- replay progress
- replay speed
- current timestamp

---

### Metrics View
Display:
- throughput
- queue occupancy
- average latency (approximate in Phase 1)

Phase 1 timing uses std::chrono and should NOT be treated as authoritative latency measurement.
Precise RDTSCP timing begins in Phase 2.

---

## Update Frequency

Recommended:

```text
100ms–500ms
```

---

## CRITICAL ARCHITECTURAL RULE

Dashboard MUST NEVER:
- block engine threads
- directly access hot-path memory
- hold engine locks

Correct architecture:

```text
Engine
    ↓
Snapshot Publisher
    ↓
WebSocket Server
    ↓
React Dashboard
```

---

# 15. Logging Requirements

## Objective
Provide debugging visibility.

---

## Rules

### Logging MUST
- be asynchronous
- remain outside hot path
- avoid blocking

### Logging MUST NOT
- use std::cout in engine threads
- block queue processing

---

# 16. Metrics Requirements

## Phase 1 Metrics

Track:
- throughput
- queue occupancy
- average latency
- replay progress

---

## Timing Method

Acceptable initially:
- std::chrono

RDTSCP timing begins in Phase 2.

---

# 17. Testing Requirements

## Unit Testing

Required coverage:

### Queue Tests
- FIFO ordering
- capacity handling
- enqueue/dequeue correctness

### Order Book Tests
- insert correctness
- cancel correctness
- spread correctness
- best bid/ask correctness

### Matching Tests
- full fills
- partial fills
- STP correctness

---

# Integration Testing

Required flow:

```text
Replay
→ Decode
→ Order Book
→ Strategy
→ Matching
→ Execution
→ Persistence
```

---

## Determinism Requirement

Same replay input MUST produce:
- same executions
- same ordering
- same replay statistics

Every run.

---

## Required Edge Case Tests

The test suite MUST include:

- empty order book matching attempt
- cancel of non-existent order
- modify of already-filled order
- replay file with out-of-order timestamps
- out-of-range price rejection
- STP-triggered cancellation validation

---

# 18. Sanitizer Requirements

## Required Sanitizers

### AddressSanitizer
Detect:
- invalid memory access
- leaks

### ThreadSanitizer
Detect:
- races
- incorrect synchronization

### UBSan
Detect:
- undefined behavior

---

## IMPORTANT

Sanitizers are REQUIRED during development.

They are not optional.

---

## Separate Sanitizer Build Targets

ThreadSanitizer and AddressSanitizer cannot run simultaneously.

CMake MUST support separate targets:

```text
cmake -DSANITIZER=address ..
cmake -DSANITIZER=thread ..
cmake -DSANITIZER=undefined ..
```

Each sanitizer configuration must build independently.

---

# 19. Definition of Done (Phase 1)

Phase 1 is complete when:

---

## Functional Requirements

### Engine
- deterministic replay works
- order matching works
- strategy stage integrated
- persistence operational

---

## Dashboard
- order book visible
- trades visible
- metrics visible
- replay visualization functional

---

## Testing
- unit tests passing
- integration tests passing
- sanitizer builds passing

---

## Determinism

Repeated replay runs produce:
- identical executions
- identical ordering
- identical statistics

---

# 20. Explicitly Deferred to Later Phases

The following are NOT Phase 1 goals:

---

## Deferred to Phase 2
- memory pools
- cache alignment
- deterministic timing
- thread pinning
- multi-symbol routing

---

## Deferred to Phase 3
- prefetching
- branch minimization
- cache optimization
- Linux low-latency tuning
- benchmark enforcement

---

## Deferred to Phase 4
- 24-hour soak testing
- advanced overload handling
- backtest orchestration
- production operations dashboard

---

## Deferred to Phase 5
- DPDK
- AF_XDP
- advanced strategies
- SIMD
- advanced matching modes

---

# 21. Recommended Implementation Order

Build in this exact order:

```text
1. Event model
2. SPSC queue
3. Order book
4. Matching engine + basic STP
5. Strategy engine
6. Dashboard
7. Replay engine
8. PostgreSQL async writer
9. Integration tests
```

This sequence minimizes:
- integration complexity
- debugging difficulty
- architectural refactors

---

# 22. Phase 1 Implementation Decisions & Additions

The following were built during Phase 1 but were not explicitly specified.
They are documented here so Phase 2 does not re-derive them.

---

## Event Model

### Side encoded in bit 16 of `Event.type`

The spec's `Event` struct has no `side` field.
Side is packed into bit 16 of the `type` field so the struct stays 40 bytes.

```
type field bit layout:
  [15:0]   EventType value (1–5)
  [16]     Side: 0 = Bid, 1 = Ask
  [31:17]  reserved (0)
```

Helper functions decode it without touching the struct layout:

```cpp
inline EventType event_type(const Event& e) noexcept;
inline Side      event_side(const Event& e) noexcept;
inline uint32_t  encode_type(EventType t, Side s = Side::Bid) noexcept;
```

Two additional static_asserts were added beyond the size check:

```cpp
static_assert(std::is_trivially_copyable_v<Event>, ...);
static_assert(std::is_standard_layout_v<Event>,    ...);
```

---

## CSV Replay Format — Extended

The spec's format has 6 fields. A 7th optional field `side` was added:

```text
timestamp,symbol_id,event_type,price,quantity,order_id[,side]
```

`side`: 0 = Bid (default when omitted), 1 = Ask.
`SequentialReplayController` parses both 6-field and 7-field rows.

---

## Replay Engine

### `VectorReplayController`

An in-memory implementation of `ReplayController` backed by `std::vector<Event>`.
Not in the spec; added for integration tests (avoids CSV I/O, makes fixtures self-contained).

### Out-of-order timestamp detection

`SequentialReplayController` tracks the previous timestamp and increments an
`out_of_order_count_` counter when a row arrives out of sequence.
Two methods were added (not in the `ReplayController` base interface):

```cpp
bool     had_out_of_order_timestamps() const noexcept;
uint64_t out_of_order_count()          const noexcept;
```

File order remains the canonical replay sequence — events are never reordered.

---

## Matching Engine

### STP mechanism — participant ID in `order_id`

The spec says STP must cancel the aggressor but does not specify how participants
are identified. Implementation encodes participant ID in the upper 16 bits of
`order_id`:

```
order_id layout: [63:48] participant_id | [47:0] sequence
```

Participant 0 is "anonymous" — STP is skipped for it.
Two helpers are provided:

```cpp
inline uint16_t participant_of(uint64_t order_id) noexcept;
inline uint64_t make_order_id(uint16_t participant_id, uint64_t sequence) noexcept;
```

### `MatchOutput` return type

`process_new_order` returns a `MatchOutput` struct (not raw events):

```cpp
enum class MatchResult : uint8_t { Ok, STPTriggered, OrderRejected };

struct MatchOutput {
    MatchResult        result;
    std::vector<Event> events;  // TradeExecution + optional CancelOrder on STP
};
```

---

## Strategy Engine

### `NullStrategy`

Drops every event. Not in spec; added for testing NOOP path and future benchmarks.

### `EventTypeFilterStrategy`

Forwards only the event types in an allowed set; NOOPs everything else.
Useful for filtering `MarketUpdate` events before they reach the matcher.

```cpp
EventTypeFilterStrategy filter({EventType::NewOrder, EventType::CancelOrder});
```

---

## Persistence Layer

### `WITH_POSTGRES` compile guard

`PostgresWriter` compiles as no-ops when `libpq` is absent.
CMake detects via `find_package(PostgreSQL)` and sets `-DWITH_POSTGRES=1` only when found.
This makes the full engine build on machines without PostgreSQL installed.

### `PostgresWriter::flush()`

Synchronous drain: pushes a `Flush` sentinel onto the async queue and
spin-waits on a `shared_ptr<atomic_bool>`. Required for clean teardown and
deterministic test assertions. Not in spec.

### UUID v4 generation

Run IDs use UUID v4 generated with `std::mt19937` + `std::uniform_int_distribution`.
No external dependency (libuuid not required).

---

## Metrics

### Latency measurement scope

`avg_latency_us` covers only the core matching path:
strategy dispatch + order book update + trade output collection.
Dashboard snapshot publishing (mutex acquisition, `top_bids/top_asks` vector
allocation) is explicitly excluded from the timer.

The spec notes that Phase 1 timing is approximate and should not be treated as
authoritative. This scoping makes the number more meaningful while remaining
consistent with that caveat.

---

## Testing

### Conditional PostgreSQL test with `GTEST_SKIP()`

`PostgresPersistence.WritesTradeToDatabase` uses `GTEST_SKIP()` when PostgreSQL
is not reachable. This allows the full suite to pass in CI without Docker.
To exercise the DB path: `docker compose up -d` then re-run ctest.

### Test fixtures directory

```text
tests/fixtures/
  integration_cross.csv   — 4 orders producing exactly 2 TradeExecution events
  out_of_order.csv        — replay with intentional timestamp regression
```

---

# 23. Final Phase 1 Goal

By the end of Phase 1, the project should behave as:

```text
A deterministic, replayable, fully integrated trading engine MVP
with visualization, persistence, and correctness validation.
```

This creates the correct foundation for:
- deterministic runtime hardening
- low-latency optimization
- benchmark enforcement
- production reliability
in later phases.

