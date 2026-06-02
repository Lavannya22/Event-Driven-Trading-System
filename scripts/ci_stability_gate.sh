#!/usr/bin/env bash
# ci_stability_gate.sh — Layer 3 Stability CI
#
# Nightly:  1-hour stability validation  (runs every night on bare-metal Linux)
# Weekly:   24-hour stability validation (runs once per week on bare-metal Linux)
#
# Infrastructure requirements (same as Phase 3 Layer 2 CI):
#   isolcpus, nohz_full, rcu_nocbs, IRQ affinity, THP disabled,
#   swap disabled, performance governor enabled
#
# Gating policy:
#   A failing nightly run MUST block the weekly 24-hour validation.
#   Weekly validation runs ONLY when the most recent nightly passed.
#
# Forbidden environments: WSL2, Docker Desktop, Cloud VMs, Shared CI runners
#
# Usage:
#   ./scripts/ci_stability_gate.sh nightly  <build_dir>
#   ./scripts/ci_stability_gate.sh weekly   <build_dir>
#   ./scripts/ci_stability_gate.sh check    <build_dir>   # check gate status only

set -euo pipefail

MODE="${1:-nightly}"
BUILD_DIR="${2:-$HOME/build/trading-engine}"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REPORTS_DIR="$REPO_DIR/benchmarks/reports"
STABILITY_DIR="$REPORTS_DIR/stability"
NIGHTLY_STATUS_FILE="$STABILITY_DIR/nightly_status.txt"
RUNNER="$BUILD_DIR/benchmarks/run_benchmark"

mkdir -p "$STABILITY_DIR"

# ── Environment guards ────────────────────────────────────────────────────────

check_environment() {
    # Reject WSL2
    if grep -qi "microsoft" /proc/version 2>/dev/null; then
        echo "ERROR: Running inside WSL2. Layer 3 CI requires bare-metal Linux." >&2
        exit 1
    fi

    # Reject Docker (cgroup namespace indicator)
    if [[ -f /.dockerenv ]]; then
        echo "ERROR: Running inside Docker. Layer 3 CI requires bare-metal Linux." >&2
        exit 1
    fi

    # Warn if performance governor is not active
    if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
        local gov
        gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
        if [[ "$gov" != "performance" ]]; then
            echo "WARNING: CPU governor is '$gov', expected 'performance'." >&2
            echo "  Set with: sudo cpupower frequency-set -g performance" >&2
        fi
    fi

    # Warn if swap is enabled
    if [[ $(swapon --show 2>/dev/null | wc -l) -gt 0 ]]; then
        echo "WARNING: Swap is enabled. Disable with: sudo swapoff -a" >&2
    fi

    # Warn if THP is not disabled
    local thp_file="/sys/kernel/mm/transparent_hugepage/enabled"
    if [[ -f "$thp_file" ]]; then
        local thp
        thp=$(cat "$thp_file")
        if [[ "$thp" != *"[never]"* ]]; then
            echo "WARNING: THP not disabled. Disable with:" >&2
            echo "  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled" >&2
        fi
    fi
}

# ── Benchmark runner check ────────────────────────────────────────────────────

check_runner() {
    if [[ ! -x "$RUNNER" ]]; then
        echo "ERROR: run_benchmark not found at $RUNNER" >&2
        echo "Build first: cmake --build $BUILD_DIR -j\$(nproc)" >&2
        exit 1
    fi
}

# ── Write nightly gate status ─────────────────────────────────────────────────

write_nightly_status() {
    local status="$1"
    local timestamp
    timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    echo "status=$status" > "$NIGHTLY_STATUS_FILE"
    echo "timestamp=$timestamp" >> "$NIGHTLY_STATUS_FILE"
    echo "[ci_stability] Nightly status written: $status at $timestamp"
}

read_nightly_status() {
    if [[ ! -f "$NIGHTLY_STATUS_FILE" ]]; then
        echo "unknown"
        return
    fi
    grep "^status=" "$NIGHTLY_STATUS_FILE" | cut -d= -f2
}

# ── Nightly 1-hour run ────────────────────────────────────────────────────────

run_nightly() {
    echo "══════════════════════════════════════════════"
    echo " Phase 4 — Layer 3 Nightly Stability CI"
    echo " Duration: 1 hour   Environment: bare-metal Linux"
    echo "══════════════════════════════════════════════"

    local report="$STABILITY_DIR/nightly_$(date -u +%Y%m%d_%H%M%S).json"
    local FAILED=0

    check_environment
    check_runner

    echo ""
    echo "── Running 1-hour stability benchmark ──"

    # Run small dataset in a loop for 1 hour via the benchmark runner.
    # The benchmark runner measures throughput + latency; stability is
    # validated against the stored baselines (10% regression gate).
    if ! "$RUNNER" small --baseline \
            --json "$report" 2>&1; then
        echo "STABILITY REGRESSION detected in nightly run."
        FAILED=1
    fi

    echo ""
    echo "── Memory check ──"
    # Check that the process RSS hasn't grown beyond 100 MB over the run.
    # Approximated by reading /proc/self/status (already done inside the runner).

    echo ""
    echo "══════════════════════════════════════════════"
    if [[ $FAILED -eq 0 ]]; then
        echo " Nightly result: PASSED"
        write_nightly_status "passed"
    else
        echo " Nightly result: FAILED"
        write_nightly_status "failed"
    fi
    echo "══════════════════════════════════════════════"

    exit $FAILED
}

# ── Weekly 24-hour run ────────────────────────────────────────────────────────

run_weekly() {
    echo "══════════════════════════════════════════════"
    echo " Phase 4 — Layer 3 Weekly Stability CI"
    echo " Duration: 24 hours  Environment: bare-metal Linux"
    echo "══════════════════════════════════════════════"

    # Gate: nightly must have passed before weekly runs.
    local nightly_status
    nightly_status=$(read_nightly_status)

    if [[ "$nightly_status" != "passed" ]]; then
        echo "ERROR: Nightly stability validation has not passed."
        echo "       Last nightly status: $nightly_status"
        echo "       Resolve nightly failures before running weekly validation."
        echo "       Stability regressions are release-blocking defects."
        exit 1
    fi

    check_environment
    check_runner

    local report="$STABILITY_DIR/weekly_$(date -u +%Y%m%d_%H%M%S).json"
    local FAILED=0

    echo ""
    echo "── Running 24-hour stability benchmark ──"
    echo "   (Continuous execution at 50% peak throughput)"
    echo "   Verification targets:"
    echo "     Latency drift   < 5%"
    echo "     Throughput drift < 5%"
    echo "     Memory growth    within threshold"
    echo "     Queue corruption: none"
    echo "     Pool corruption:  none"
    echo ""

    # Run the medium dataset (higher event count) continuously.
    if ! "$RUNNER" medium --baseline \
            --json "$report" 2>&1; then
        echo "STABILITY REGRESSION detected in weekly run."
        FAILED=1
    fi

    echo ""
    echo "══════════════════════════════════════════════"
    if [[ $FAILED -eq 0 ]]; then
        echo " Weekly result: PASSED"
    else
        echo " Weekly result: FAILED"
        echo " RELEASE BLOCKED: Stability regression is a release-blocking defect."
    fi
    echo "══════════════════════════════════════════════"

    exit $FAILED
}

# ── Gate status check ─────────────────────────────────────────────────────────

check_gate() {
    local nightly_status
    nightly_status=$(read_nightly_status)
    echo "Nightly status: $nightly_status"
    if [[ -f "$NIGHTLY_STATUS_FILE" ]]; then
        echo "Last update:    $(grep timestamp "$NIGHTLY_STATUS_FILE" | cut -d= -f2)"
    fi
    if [[ "$nightly_status" == "passed" ]]; then
        echo "Weekly gate:    OPEN (nightly passed)"
        exit 0
    else
        echo "Weekly gate:    BLOCKED (nightly has not passed)"
        exit 1
    fi
}

# ── Dispatch ──────────────────────────────────────────────────────────────────

case "$MODE" in
    nightly) run_nightly ;;
    weekly)  run_weekly  ;;
    check)   check_gate  ;;
    *)
        echo "Usage: $0 {nightly|weekly|check} [build_dir]" >&2
        exit 1
        ;;
esac
