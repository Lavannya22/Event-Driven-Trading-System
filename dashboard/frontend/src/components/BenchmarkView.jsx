import { useState, useEffect } from 'react'

const BASELINE_URL = null  // baselines served statically if needed

export default function BenchmarkView({ lastReport = null }) {
  // lastReport comes from the WebSocket snapshot (injected by backend after a run)
  // For now show the static small baseline for comparison
  const [baseline, setBaseline] = useState(null)

  // Hardcoded WSL2 baseline values (informational)
  const wsl2Baseline = {
    name: 'small (WSL2 informational)',
    throughput: 1071380,
    p50: 11,
    p95: 23,
    p99: 47,
    p999: 383,
    max: 46219,
  }

  const report = lastReport || wsl2Baseline

  const rows = [
    { label: 'Throughput',   key: 'throughput', unit: 'events/s', format: v => (v / 1e6).toFixed(2) + 'M', higherBetter: true  },
    { label: 'p50',          key: 'p50',         unit: 'ns',      format: v => v?.toLocaleString(),         higherBetter: false },
    { label: 'p95',          key: 'p95',         unit: 'ns',      format: v => v?.toLocaleString(),         higherBetter: false },
    { label: 'p99',          key: 'p99',         unit: 'ns',      format: v => v?.toLocaleString(),         higherBetter: false },
    { label: 'p99.9',        key: 'p999',        unit: 'ns',      format: v => v?.toLocaleString(),         higherBetter: false },
    { label: 'max',          key: 'max',         unit: 'ns',      format: v => v?.toLocaleString(),         higherBetter: false },
  ]

  const getRegressionPct = (current, base, higherBetter) => {
    if (!base || !current) return null
    const ratio = current / base
    return higherBetter ? (ratio - 1) * 100 : (1 - ratio) * 100
  }

  return (
    <div className="space-y-8">

      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Benchmark Comparison — Current vs Baseline
        </h3>

        <div className="overflow-x-auto">
          <table className="w-full text-sm">
            <thead>
              <tr className="text-xs text-gray-500 uppercase border-b border-gray-800">
                <th className="text-left pb-2 pr-6">Metric</th>
                <th className="text-right pb-2 pr-6">Current</th>
                <th className="text-right pb-2 pr-6">Baseline</th>
                <th className="text-right pb-2 pr-6">∆</th>
                <th className="text-left pb-2">Status</th>
              </tr>
            </thead>
            <tbody>
              {rows.map(row => {
                const cur  = report?.[row.key]
                const base = wsl2Baseline?.[row.key]
                const pct  = getRegressionPct(cur, base, row.higherBetter)
                const isRegression = pct !== null && pct < -10
                const isImprovement = pct !== null && pct > 0

                return (
                  <tr key={row.key} className="border-b border-gray-900">
                    <td className="py-2 pr-6 text-gray-300">{row.label}</td>
                    <td className="py-2 pr-6 text-right tabular-nums text-white font-medium">
                      {cur != null ? row.format(cur) : '—'} <span className="text-gray-600 text-xs">{row.unit}</span>
                    </td>
                    <td className="py-2 pr-6 text-right tabular-nums text-gray-500">
                      {base != null ? row.format(base) : '—'} <span className="text-gray-700 text-xs">{row.unit}</span>
                    </td>
                    <td className={`py-2 pr-6 text-right tabular-nums text-xs font-semibold ${
                      isRegression ? 'text-red-400' : isImprovement ? 'text-green-400' : 'text-gray-500'
                    }`}>
                      {pct != null ? `${pct > 0 ? '+' : ''}${pct.toFixed(1)}%` : '—'}
                    </td>
                    <td className="py-2">
                      {isRegression
                        ? <span className="text-xs text-red-400 font-semibold">REGRESSION</span>
                        : isImprovement
                          ? <span className="text-xs text-green-400">improved</span>
                          : <span className="text-xs text-gray-600">—</span>}
                    </td>
                  </tr>
                )
              })}
            </tbody>
          </table>
        </div>

        <p className="text-xs text-gray-600 mt-4">
          Baseline: WSL2 informational run (2026-05-30). CI gate triggers on &gt;10% regression.
          Update baseline with <code className="text-blue-400">scripts/update_baseline.sh</code> after optimization.
        </p>
      </section>

      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">CI Gate Rules</h3>
        <div className="grid grid-cols-2 gap-4 max-w-md">
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 mb-1">Throughput regression limit</div>
            <div className="text-lg font-bold text-red-400">10%</div>
          </div>
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 mb-1">Latency regression limit</div>
            <div className="text-lg font-bold text-red-400">10%</div>
          </div>
        </div>
        <p className="text-xs text-gray-600 mt-3">
          Run: <code className="text-blue-400">./scripts/ci_benchmark_gate.sh ~/build/trading-engine</code>
        </p>
      </section>

    </div>
  )
}
