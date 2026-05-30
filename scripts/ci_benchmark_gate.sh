#!/usr/bin/env bash
# ci_benchmark_gate.sh — Layer 2 benchmark CI validation.
#
# Runs the benchmark suite and compares results against stored baselines.
# Exits non-zero (fails the build) if throughput or latency regresses >10%.
#
# Usage: ./scripts/ci_benchmark_gate.sh <build_dir>
# Example: ./scripts/ci_benchmark_gate.sh ~/build/trading-engine

set -euo pipefail

BUILD_DIR="${1:-$HOME/build/trading-engine}"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DATASETS_DIR="$REPO_DIR/benchmarks/datasets"
BASELINES_DIR="$REPO_DIR/benchmarks/baselines"
REPORTS_DIR="$REPO_DIR/benchmarks/reports"
RUNNER="$BUILD_DIR/benchmarks/run_benchmark"

if [[ ! -x "$RUNNER" ]]; then
    echo "ERROR: run_benchmark not found at $RUNNER" >&2
    echo "Run: cmake --build $BUILD_DIR -j\$(nproc)" >&2
    exit 1
fi

# Generate datasets if missing
if [[ ! -f "$DATASETS_DIR/small.csv" ]]; then
    echo "Generating benchmark datasets..."
    "$BUILD_DIR/benchmarks/gen_datasets" "$DATASETS_DIR"
fi

mkdir -p "$REPORTS_DIR"

FAILED=0
DATASETS=("small" "medium")

echo "══════════════════════════════════════════════"
echo " Phase 3 — Layer 2 Benchmark CI Gate"
echo "══════════════════════════════════════════════"

for ds in "${DATASETS[@]}"; do
    echo ""
    echo "── Running: $ds ──"

    REPORT="$REPORTS_DIR/${ds}_ci.json"
    BASELINE="$BASELINES_DIR/baseline_${ds}.json"

    "$RUNNER" "$ds" --json "$REPORT" || { echo "Benchmark run failed for $ds"; FAILED=1; continue; }

    if [[ -f "$BASELINE" ]]; then
        if ! "$RUNNER" "$ds" --baseline 2>&1; then
            echo "REGRESSION DETECTED in $ds"
            FAILED=1
        fi
    else
        echo "No baseline for $ds — skipping comparison. Run scripts/update_baseline.sh to set one."
    fi
done

echo ""
echo "══════════════════════════════════════════════"
if [[ $FAILED -eq 0 ]]; then
    echo " Result: PASSED"
else
    echo " Result: FAILED — performance regression detected"
fi
echo "══════════════════════════════════════════════"

exit $FAILED
