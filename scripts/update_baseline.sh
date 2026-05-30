#!/usr/bin/env bash
# update_baseline.sh — Manually update a benchmark baseline.
#
# Usage:
#   ./scripts/update_baseline.sh <report.json> <baselines_dir>
#
# Example:
#   ./scripts/update_baseline.sh benchmarks/reports/small_latest.json benchmarks/baselines
#
# Baselines must be updated manually — automatic updates are forbidden.
# Every update requires a dedicated git commit:
#   perf: update benchmark baseline after <optimization-name>

set -euo pipefail

REPORT="${1:-}"
BASELINES_DIR="${2:-benchmarks/baselines}"

if [[ -z "$REPORT" ]]; then
    echo "Usage: $0 <report.json> [baselines_dir]" >&2
    exit 1
fi

if [[ ! -f "$REPORT" ]]; then
    echo "Error: report file not found: $REPORT" >&2
    exit 1
fi

# Extract name field to determine target baseline file
NAME=$(grep '"name"' "$REPORT" | sed 's/.*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
TARGET="$BASELINES_DIR/baseline_${NAME}.json"

echo "Updating baseline: $TARGET"
echo "Source report:     $REPORT"

# Confirm
read -p "Confirm update? [y/N] " confirm
[[ "$confirm" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 0; }

# Extract key metrics from the report and write a clean baseline JSON
THROUGHPUT=$(grep '"throughput"' "$REPORT" | grep -o '[0-9.]*' | head -1)
P99=$(grep '"p99"'  "$REPORT" | grep -o '[0-9]*' | head -1)
P999=$(grep '"p999"' "$REPORT" | grep -o '[0-9]*' | head -1)
P50=$(grep '"p50"'  "$REPORT" | grep -o '[0-9]*' | head -1)
P95=$(grep '"p95"'  "$REPORT" | grep -o '[0-9]*' | head -1)
MAX=$(grep '"max"'  "$REPORT" | grep -o '[0-9]*' | head -1)
TS=$(grep '"timestamp"' "$REPORT" | sed 's/.*"timestamp"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')

mkdir -p "$BASELINES_DIR"
cat > "$TARGET" <<EOF
{
  "name": "$NAME",
  "baseline_set_at": "$TS",
  "note": "Set from $REPORT — commit with: perf: update benchmark baseline after <optimization>",
  "throughput": $THROUGHPUT,
  "p50": $P50,
  "p95": $P95,
  "p99": $P99,
  "p999": $P999,
  "max": $MAX
}
EOF

echo "Baseline written to: $TARGET"
echo ""
echo "Next steps:"
echo "  git add $TARGET"
echo "  git commit -m 'perf: update benchmark baseline after <optimization>'"
