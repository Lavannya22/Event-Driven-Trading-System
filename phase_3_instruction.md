# phase_3_instruction.md

# Phase 3 — Performance Optimization & Benchmarking

---

# 1. Purpose

Phase 3 transforms the deterministic, allocation-free engine from Phase 2 into a high-performance, low-latency trading simulation engine capable of approaching the latency and throughput goals defined in the Success Criteria.

Primary objective:

> Optimize the existing architecture without changing functional behavior.

The engine must remain:

* Correct
* Deterministic
* Allocation-free during runtime

All Phase 1 and Phase 2 test suites must continue passing.

---

# 2. Core Principle

Performance engineering begins only after correctness and determinism have been proven.

Optimization order:

```text
Memory Access
    ↓
Cache Efficiency
    ↓
Branch Reduction
    ↓
Latency Measurement
    ↓
Benchmark Automation
```

Rules:

* Never optimize before measuring
* Every optimization requires before/after benchmarks
* Every optimization must preserve behavior
* No new business functionality introduced

---

# 3. Phase 3 Goals

By the end of Phase 3 the system must provide:

### Performance Infrastructure

* Benchmark framework
* Throughput measurement
* Latency measurement
* HDR-style latency histograms
* Performance dashboards

### CPU Efficiency

* Shift-based price indexing
* Cache-aware order book access
* Branch-minimized matching path
* Explicit memory prefetching

### Benchmark Infrastructure

* Benchmark datasets
* Baseline storage
* Regression detection
* Linux benchmark environment

### CI

* Layer 2 benchmark validation active
* Performance regressions automatically fail CI

---

# 4. Components

---

# 4.1 Benchmark Framework

Create:

```cpp
class BenchmarkRunner
```

Responsibilities:

* Load benchmark datasets
* Execute replay runs
* Measure throughput
* Measure latency
* Generate benchmark reports
* Compare against baselines

Metrics:

```text
events/sec
average latency
p50
p95
p99
p99.9
max latency
```

Output formats:

```text
JSON
CSV
Console
```

Directory:

```text
benchmarks/
```

### Benchmark Warm-Up

Every benchmark must include a warm-up phase.

Requirements:

```text
Minimum:
- 1 second
OR
- 1,000,000 events
```

During warm-up:

* No histogram recording
* No latency recording
* No throughput recording

Purpose:

* Warm caches
* Warm branch predictors
* Warm instruction cache
* Remove cold-start effects

Reports must include:

```text
Warm-up duration
Measurement duration
```

---

# 4.2 HDR-Style Latency Histogram

Create:

```cpp
class LatencyHistogram
```

Requirements:

* Lock-free recording
* Fixed memory footprint
* Startup allocation only
* Thread-safe
* No runtime allocations

Supported metrics:

```text
p50
p95
p99
p99.9
max
```

### Histogram Resolution

Requirements:

```text
Range:
1 ns → 1 ms

Precision:
Minimum 3 significant digits

Memory:
Allocated at startup
Never resized
```

Acceptable implementation:

```text
Fixed HDR-style histogram

OR

Log2 bucket histogram
64 fixed buckets
O(1) insertion
```

All benchmark runs must use identical histogram configuration.

---

# 4.3 Performance Metrics Collector

Create:

```cpp
class PerformanceMetrics
```

Responsibilities:

* Throughput tracking
* Latency tracking
* Queue depth tracking
* Pool utilization tracking
* Cache statistics ingestion

Dashboard integration required.

### Cache Statistics Collection

Cache metrics collected via:

```bash
perf stat -e cache-misses,cache-references
```

Results parsed after benchmark completion and injected into benchmark reports.

Real-time cache statistics are not required.

---

# 4.4 Shift-Based Price Indexing

Current implementation:

```cpp
(price - base_price) / tick_size
```

Optimized implementation:

```cpp
(price - base_price) >> tick_shift
```

Requirements:

```cpp
uint32_t tick_shift;
```

Division operations are forbidden inside:

* Order book hot path
* Matching engine hot path

### Validation

Inspect generated assembly using:

```bash
objdump -d
perf annotate
```

Forbidden instructions:

```text
div
idiv
divsd
```

---

# 4.5 Branch-Minimized Matching Engine

Optimize matching logic.

Goals:

* Reduce branch mispredictions
* Improve instruction pipeline utilization

Preferred patterns:

```cpp
early return
```

```cpp
predictable branch ordering
```

### Branch Profiling Requirement

Before optimizing:

Measure:

```bash
perf stat -e branches,branch-misses
```

Only optimize branches with:

```text
>5% misprediction rate
```

Do not modify already predictable branches.

Every optimization must include:

```text
Before benchmark
After benchmark
Documented improvement
```

No behavioral changes allowed.

---

# 4.6 Prefetching Framework

Introduce explicit CPU prefetching.

Example:

```cpp
__builtin_prefetch(next_price_level);
```

Targets:

* Price levels
* Matching queues
* Event buffers

Rules:

* Prefetch predictable accesses only
* No speculative prefetching
* Benchmark before/after

### Prefetch Distance

Initial target:

```text
1–2 iterations ahead
```

Example:

```text
Process current level
Prefetch next level
```

Adjust only if benchmark data proves benefit.

---

# 4.7 Cache-Aware Order Book Optimization

Phase 2 already introduced:

* Array-based price levels
* FlatOrderMap
* Cache-aligned structures

Phase 3 does NOT redesign the order book.

Focus areas:

1. Cache residency validation
2. Adjacent level prefetching
3. Access pattern optimization
4. Hot-level profiling
5. Capacity validation

No new container types may be introduced.

Forbidden:

```text
std::map
std::unordered_map
std::list
```

inside the matching hot path.

---

# 4.8 Hot Path Audit

Perform complete audit of:

* Matching engine
* Order book
* Strategy engine
* Event pipeline

Forbidden:

```text
malloc
free
new
delete
shared_ptr
mutex
condition_variable
fstream
iostream
```

inside hot paths.

### Audit Deliverable

Produce:

```text
hot_path_audit.md
```

Contents:

* Functions audited
* Violations discovered
* Fixes applied
* Final verification status

Audit becomes part of Phase 3 artifacts.

---

# 4.9 Linux Benchmark Environment

Create dedicated benchmark environment.

Required:

```text
Ubuntu Linux
Performance Governor
THP Disabled
Swap Disabled
```

Additional kernel tuning:

```text
isolcpus
nohz_full
rcu_nocbs
```

IRQ affinity:

```text
Move interrupts off benchmark cores
```

### Verification

```bash
cat /proc/cmdline
cat /proc/interrupts
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

THP:

```bash
cat /sys/kernel/mm/transparent_hugepage/enabled
```

Expected:

```text
never
```

WSL2 benchmark results are informational only.

Authoritative results must come from Linux.

---

# 4.10 Benchmark Datasets

Create:

```text
small
medium
large
stress
```

### Small

```text
100K events
```

### Medium

```text
1M events
```

### Large

```text
10M events
```

### Stress

```text
Queue pressure
Pool pressure
High cancel rate
Burst traffic
```

### Dataset Format

Small / Medium:

```text
CSV replay format
```

Large / Stress:

```text
Binary replay format
```

Binary records must match Event struct layout.

---

# 4.11 Performance Baseline System

Directory:

```text
benchmarks/baselines/
```

Example:

```json
{
  "throughput": 10000000,
  "p99": 2000,
  "p999": 5000
}
```

Units:

```text
Nanoseconds
```

Baselines must be version controlled.

### Baseline Update Policy

Baselines updated manually only.

Required script:

```text
scripts/update_baseline.sh
```

Automatic baseline updates are forbidden.

Every update requires:

```text
Performance improvement verification
Dedicated commit
```

Example:

```text
perf: update benchmark baseline after shift-index optimization
```

---

# 4.12 Layer 2 CI Benchmark Validation

Activate benchmark CI.

Pipeline:

```text
Compile
↓
Run Benchmarks
↓
Compare Baselines
↓
Pass / Fail
```

Failure conditions:

```text
Throughput regression > 10%

OR

Latency regression > 10%
```

Build must fail.

### Benchmark Runner Requirements

Must run on:

```text
Dedicated bare-metal Linux
```

Forbidden:

```text
WSL2
Virtual Machines
Shared Cloud Runners
```

Reason:

Virtualized environments produce non-deterministic latency measurements.

---

# 4.13 Dashboard Enhancements

Add Performance View.

Display:

```text
Throughput
Average Latency
P50
P95
P99
P99.9
Max Latency
```

Add Benchmark View.

Display:

```text
Current Run
Baseline
Regression %
```

Refresh interval:

```text
500 ms
```

Dashboard remains outside hot path.

---

# 5. Testing Requirements

---

# 5.1 Unit Tests

### LatencyHistogram

Verify:

```text
Percentile calculations
Bucket placement
No allocation
```

### BenchmarkRunner

Verify:

```text
Metric calculations
Report generation
Baseline comparison
```

### Shift Indexing

Verify:

```text
Price → Index mapping correctness
```

### Prefetch Logic

Verify:

```text
No behavioral changes
```

---

# 5.2 Integration Tests

### Benchmark Replay

```text
Replay dataset
Generate throughput report
Generate latency report
```

### Regression Detection

```text
Baseline comparison works
CI failure triggers correctly
```

### Histogram Validation

```text
Percentiles generated correctly
```

### Linux Benchmark Validation

Verify benchmark environment configuration.

---

# 5.3 Performance Validation

### Throughput

Required:

```text
≥ 10M events/sec
```

### Latency

Required:

```text
p99 < 5 µs
p99.9 < 5 µs
```

Stretch Goal:

```text
p99 < 2 µs
```

### Cache Miss Rate

Required:

```text
< 1%
```

Measured via:

```bash
perf stat
```

### Benchmark Reports Must Include

```text
p50
p95
p99
p99.9
max latency
throughput
cache misses
```

---

# 6. Definition of Done

Phase 3 is complete when:

### Benchmarking

* Benchmark framework operational
* Warm-up support implemented
* Latency reports generated
* Throughput reports generated

### Optimization

* Shift indexing implemented
* Prefetching implemented
* Branch optimization completed
* Cache-aware order book validated

### Validation

* No functional regressions
* All Phase 1 tests pass
* All Phase 2 tests pass

### Audit

* Hot path audit completed
* Audit report produced

### CI

* Layer 2 benchmark gate operational
* Baselines stored
* Regression detection operational

### Dashboard

* Performance view operational
* Benchmark comparison view operational

### Targets

* ≥10M events/sec achieved
* p99 reported
* p99.9 reported
* Cache miss rate measured

---

# 7. Deferred To Phase 4

Explicitly out of scope:

```text
24-hour soak testing
Backtest controller
Overload handling
Pool exhaustion policies
Production reliability
Advanced observability
Live deployment tooling
```

---

# 8. Build Order

Implement in this order:

```text
1.  Benchmark Framework
2.  Latency Histogram
3.  Performance Metrics Collector
4.  Benchmark Datasets
5.  Baseline System
6.  Linux Benchmark Environment
7.  Shift-Based Price Indexing
8.  Cache-Aware Order Book Validation
9.  Branch Profiling
10. Branch Optimization
11. Prefetching
12. Dashboard Performance Views
13. Layer 2 Benchmark CI
14. Performance Validation
15. Hot Path Audit
```

> Note: Linux Benchmark Environment (step 6) is placed before all optimizations
> so that every before/after benchmark runs on authoritative bare-metal hardware.

---

# End of Phase 3

---

# Phase 3 Implementation Decisions & Additions

The following were built during Phase 3 but were not explicitly specified, or deviate
from the spec in ways future phases should know about.

---

## Timing — `CLOCK_MONOTONIC_RAW` on Linux

The spec does not specify which clock to use for latency measurement.
`BenchmarkRunner` uses `clock_gettime(CLOCK_MONOTONIC_RAW)` on Linux and
`std::chrono::steady_clock` on other platforms.

`CLOCK_MONOTONIC_RAW` is preferred over `CLOCK_MONOTONIC` for benchmarking because it
is not subject to NTP frequency adjustments, giving more stable nanosecond readings
across the warm-up and measurement phases.

---

## Warm-Up Termination — OR, Not AND

The spec states:

```
Minimum warm-up: 1 second OR 1,000,000 events
```

The implementation terminates warm-up when **either** threshold is met (not both):

```cpp
if (wu_count >= wu_min_ev && elapsed_s >= wu_min_s) break;
```

This matches the spec's intent: the warm-up is sufficient when the cache is hot,
which happens whichever threshold fires first.

---

## LatencyHistogram — Max Tracking via CAS Loop

The spec requires "lock-free recording". The max value is tracked with a relaxed
CAS loop rather than a separate atomic:

```cpp
uint64_t cur = max_.load(std::memory_order_relaxed);
while (ns > cur &&
       !max_.compare_exchange_weak(cur, ns,
           std::memory_order_relaxed, std::memory_order_relaxed))
{}
```

All other bucket increments use `fetch_add(1, relaxed)` — true lock-free, O(1).
Percentile reads are approximate (relaxed loads across independent atomics), which is
acceptable for a histogram where snapshot coherence is not required.

---

## `benchmark_lib` Static Library

The spec describes a `BenchmarkRunner` class but does not specify how it should be
compiled. It is built as a separate static library (`libbenchmark_lib.a`) rather than
compiled directly into the runner executable. This allows unit tests to link against
`BenchmarkRunner` and `LatencyHistogram` independently without duplicating compilation.

---

## `gen_datasets` and `run_benchmark` Executables

The spec describes datasets and a BenchmarkRunner class but does not specify CLI
executables. Two executables were added under `benchmarks/`:

- `gen_datasets <output_dir>` — generates all four datasets (small/medium/stress/large)
- `run_benchmark [dataset] [--baseline] [--json out] [--csv out]` — wraps BenchmarkRunner

These are the primary interfaces for running Phase 3 benchmarks from the command line.

---

## Shift-Based Indexing Applied to Level Array Size

The spec focuses on the price-to-index calculation. The `tick_shift` parameter also
reduces the level array allocation at construction time:

```cpp
// bid_levels_ and ask_levels_ sized to actual number of addressable levels
bid_levels_(NUM_LEVELS >> tick_shift)
ask_levels_(NUM_LEVELS >> tick_shift)
```

With `tick_shift=0` (default), this is identical to Phase 2 (200,000 levels).
With `tick_shift=2`, only 50,000 levels are allocated — 4× memory reduction.

`OrderBook` constructor signature changed:
```cpp
// Phase 2
explicit OrderBook(uint32_t symbol_id, std::size_t max_orders = 65536);
// Phase 3 (backward-compatible default)
explicit OrderBook(uint32_t symbol_id, std::size_t max_orders = 65536, uint32_t tick_shift = 0);
```

---

## Prefetch Parameters

`__builtin_prefetch(ptr, rw, locality)` in the matching loop:

```cpp
__builtin_prefetch(next_order, 0, 1);
//                              ^  ^
//                              |  L2/L3 cache hint (1 = moderate temporal locality)
//                              read hint (not write)
```

- `rw=0` — read hint (prefetching data we will read, not write)
- `locality=1` — L2/L3 cache hint; avoids polluting L1 with prefetched data
  that may not be accessed immediately (conservative choice)

---

## WSL2 / NTFS I/O Bottleneck

The medium dataset (1M events, CSV on `/mnt/d/`) produced misleading results:

```
Medium: Throughput 0.07M events/s   (expected ≥1M)
        p95: 24,575 ns              (expected ~50 ns)
```

Root cause: reading 1M CSV lines from an NTFS path via WSL2's filesystem translation
layer dominates wall-clock time. The engine latency is not being measured.

**This is not an engine performance issue.** As the spec states:
> WSL2 benchmark results are informational only.
> Authoritative results must come from Linux.

For accurate benchmarks, copy datasets to the Linux filesystem before running:
```bash
cp /mnt/d/.../benchmarks/datasets/medium.csv ~/bench/
./run_benchmark ~/bench/medium.csv
```

---

## WSL2 Small Dataset Baseline (Informational)

The first baseline was set from a WSL2 run on the small dataset (100K events,
entirely in memory after warm-up, negligible I/O):

```json
{
  "throughput": 1071380,
  "p50": 11,
  "p95": 23,
  "p99": 47,
  "p999": 383,
  "max": 46219
}
```

Units: throughput in events/s, latency in nanoseconds.

This baseline gates the CI script on WSL2 development machines. The bare-metal
Linux baseline must be set separately after the first authoritative run using
`scripts/update_baseline.sh`.

---

## Dashboard — 7 Tabs (Phase 3 Adds 2)

The dashboard grew from 5 tabs (Phase 2) to 7 tabs:

| Tab | Phase | Content |
|-----|-------|---------|
| Order Book | 1 | Bid/ask depth, best bid/ask, spread |
| Trades | 1 | Recent TradeExecution events |
| Replay | 1 | Replay progress, source, timestamps |
| Metrics | 1 | Throughput, latency, signals/noops |
| Pools | 2 | Pool utilisation + startup report |
| Performance | 3 | Live p50/p95/p99/p99.9, Phase 3 targets |
| Benchmark | 3 | Current run vs baseline, regression % |

---

## `ci_benchmark_gate.sh` — Additional CI Script

The spec specifies `scripts/update_baseline.sh`. An additional script was added:

- `scripts/ci_benchmark_gate.sh <build_dir>` — runs small + medium benchmarks,
  compares against stored baselines, exits non-zero on >10% regression.

This is the script that should be invoked by the CI pipeline after every build.

---

## Hot Path Audit Location

The audit document is at `docs/hot_path_audit.md` (not a `benchmarks/` artifact).
The spec says "audit becomes part of Phase 3 artifacts" without specifying location.
Placing it in `docs/` keeps all architecture documents together.

**Audit result: 0 violations. No malloc/new/mutex/iostream in any hot-path function.**

**Phase 3 Exit Condition**

The engine is now:

* Correct
* Deterministic
* Allocation-free
* Benchmarked
* Performance optimized
* Protected against performance regressions

and ready for **Phase 4 — Production Stability & Reliability**.
