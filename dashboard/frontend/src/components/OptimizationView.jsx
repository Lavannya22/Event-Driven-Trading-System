const THRESHOLD = 10  // Phase 5 spec: keep if >= 10% improvement

export default function OptimizationView({ optimization = {} }) {
  const {
    enabled    = [],  // e.g. ["NUMA","SIMD"]
    results    = [],  // GateResult[]: { category, throughput_gain_pct, latency_gain_pct, passed }
    baseline   = {},
  } = optimization

  return (
    <div className="space-y-8">

      {/* Gate rule reminder */}
      <div className="bg-gray-900 rounded-lg px-4 py-3 text-sm text-gray-400 border border-gray-800">
        Phase 5 rule: an optimization is <span className="text-white font-semibold">retained</span> only
        if throughput <span className="text-green-400">or</span> p99 latency improves by ≥
        <span className="text-yellow-300 font-semibold"> {THRESHOLD}%</span>.
        Otherwise it is <span className="text-red-400 font-semibold">removed</span>.
      </div>

      {/* Enabled optimizations */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Enabled Optimizations
        </h3>
        {enabled.length === 0 ? (
          <p className="text-gray-600 text-sm">No optimizations active (baseline mode).</p>
        ) : (
          <div className="flex flex-wrap gap-2">
            {enabled.map(opt => (
              <span key={opt}
                className="px-3 py-1 bg-blue-900/50 border border-blue-700 text-blue-300 rounded-full text-sm font-semibold">
                {opt}
              </span>
            ))}
          </div>
        )}
      </section>

      {/* Gate results table */}
      {results.length > 0 && (
        <section>
          <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
            Optimization Gate Results
          </h3>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-gray-500 uppercase border-b border-gray-800">
                  <th className="text-left pb-2 pr-6">Category</th>
                  <th className="text-right pb-2 pr-6">Throughput Δ</th>
                  <th className="text-right pb-2 pr-6">p99 Latency Δ</th>
                  <th className="text-left pb-2">Decision</th>
                </tr>
              </thead>
              <tbody>
                {results.map(r => (
                  <tr key={r.category} className="border-b border-gray-900">
                    <td className="py-2 pr-6 text-gray-300 font-semibold">{r.category}</td>
                    <td className={`py-2 pr-6 text-right tabular-nums font-semibold ${
                      r.throughput_gain_pct >= THRESHOLD ? 'text-green-400'
                      : r.throughput_gain_pct > 0 ? 'text-yellow-400' : 'text-red-400'
                    }`}>
                      {r.throughput_gain_pct >= 0 ? '+' : ''}{r.throughput_gain_pct?.toFixed(1)}%
                    </td>
                    <td className={`py-2 pr-6 text-right tabular-nums font-semibold ${
                      r.latency_gain_pct >= THRESHOLD ? 'text-green-400'
                      : r.latency_gain_pct > 0 ? 'text-yellow-400' : 'text-red-400'
                    }`}>
                      {r.latency_gain_pct >= 0 ? '+' : ''}{r.latency_gain_pct?.toFixed(1)}%
                    </td>
                    <td className="py-2">
                      {r.passed
                        ? <span className="text-green-400 font-semibold text-xs">✓ RETAIN</span>
                        : <span className="text-red-400 font-semibold text-xs">✗ REMOVE</span>}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      )}

      {/* Baseline reference */}
      {(baseline.mean_throughput || baseline.mean_p99_ns) && (
        <section>
          <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
            Baseline Reference
          </h3>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <BaselineCard label="Throughput (mean)"  value={`${(baseline.mean_throughput / 1e6)?.toFixed(2)}M`} unit="ev/s" />
            <BaselineCard label="Throughput (best)"  value={`${(baseline.best_throughput / 1e6)?.toFixed(2)}M`} unit="ev/s" />
            <BaselineCard label="p99 (mean)"         value={baseline.mean_p99_ns?.toLocaleString()} unit="ns" />
            <BaselineCard label="p99 (best)"         value={baseline.best_p99_ns?.toLocaleString()} unit="ns" />
          </div>
        </section>
      )}

    </div>
  )
}

function BaselineCard({ label, value, unit }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="text-lg font-bold text-gray-200 tabular-nums">
        {value ?? '—'}
        <span className="text-sm font-normal text-gray-500 ml-1">{unit}</span>
      </div>
    </div>
  )
}
