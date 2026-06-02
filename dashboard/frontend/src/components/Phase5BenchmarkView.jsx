const CATEGORIES = ['Baseline', 'NUMA', 'HugePages', 'SIMD', 'DPDK', 'FullOptimization']
const TARGETS = { throughput: 20e6, p99_ns: 2000, p999_ns: 3000, max_ns: 10000 }

export default function Phase5BenchmarkView({ phase5bench = {} }) {
  const { categories = {} } = phase5bench  // keyed by category name → RepStats

  const baseline = categories['Baseline']

  return (
    <div className="space-y-8">

      {/* Phase 5 targets */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Phase 5 Performance Targets
        </h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <TargetCard label="Throughput" target="≥ 20M ev/s"  met={baseline?.mean_throughput >= TARGETS.throughput} />
          <TargetCard label="p99"        target="< 2 µs"      met={baseline?.mean_p99_ns   <= TARGETS.p99_ns}  />
          <TargetCard label="p99.9"      target="< 3 µs"      met={false}  />
          <TargetCard label="max"        target="< 10 µs"     met={false}  />
        </div>
      </section>

      {/* Per-category comparison */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Category Comparison  ({Object.keys(categories).length} / {CATEGORIES.length} categories measured)
        </h3>

        {Object.keys(categories).length === 0 ? (
          <div className="bg-gray-900 rounded-lg p-6 text-center">
            <p className="text-gray-500 text-sm">
              No benchmark data yet. Run:
            </p>
            <code className="text-blue-400 text-sm block mt-2">
              ./run_advanced_benchmark --all-categories
            </code>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-gray-500 uppercase border-b border-gray-800">
                  <th className="text-left pb-2 pr-4">Category</th>
                  <th className="text-right pb-2 pr-4">Throughput (mean)</th>
                  <th className="text-right pb-2 pr-4">Throughput (best)</th>
                  <th className="text-right pb-2 pr-4">StdDev</th>
                  <th className="text-right pb-2 pr-4">p99 (mean)</th>
                  <th className="text-right pb-2">p99 (best)</th>
                </tr>
              </thead>
              <tbody>
                {CATEGORIES.filter(c => categories[c]).map(cat => {
                  const s = categories[cat]
                  const isBaseline = cat === 'Baseline'
                  const tpGain = baseline && !isBaseline
                    ? ((s.mean_throughput - baseline.mean_throughput) / baseline.mean_throughput * 100)
                    : null
                  const meetsTP = s.mean_throughput >= TARGETS.throughput
                  const meetsP99 = s.mean_p99_ns <= TARGETS.p99_ns
                  return (
                    <tr key={cat} className={`border-b border-gray-900 ${isBaseline ? 'bg-gray-900/30' : ''}`}>
                      <td className="py-2 pr-4 text-gray-300 font-semibold">
                        {cat} {isBaseline && <span className="text-xs text-gray-600 ml-1">(ref)</span>}
                      </td>
                      <td className={`py-2 pr-4 text-right tabular-nums font-semibold ${meetsTP ? 'text-green-400' : 'text-gray-300'}`}>
                        {(s.mean_throughput / 1e6).toFixed(2)}M
                        {tpGain !== null && (
                          <span className={`text-xs ml-1 ${tpGain >= 10 ? 'text-green-400' : tpGain > 0 ? 'text-yellow-400' : 'text-red-400'}`}>
                            ({tpGain >= 0 ? '+' : ''}{tpGain.toFixed(1)}%)
                          </span>
                        )}
                      </td>
                      <td className="py-2 pr-4 text-right tabular-nums text-gray-400">
                        {(s.best_throughput / 1e6).toFixed(2)}M
                      </td>
                      <td className="py-2 pr-4 text-right tabular-nums text-gray-500">
                        {(s.stddev_throughput / 1e6).toFixed(2)}M
                      </td>
                      <td className={`py-2 pr-4 text-right tabular-nums ${meetsP99 ? 'text-green-400' : 'text-gray-300'}`}>
                        {s.mean_p99_ns?.toLocaleString()} ns
                      </td>
                      <td className="py-2 text-right tabular-nums text-gray-400">
                        {s.best_p99_ns?.toLocaleString()} ns
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        )}
        <p className="text-xs text-gray-600 mt-3">
          Each category: {10} repetitions. Mean/best/stddev computed across all reps.
          Green = meets Phase 5 target. Δ% vs Baseline shown for non-baseline categories.
        </p>
      </section>

    </div>
  )
}

function TargetCard({ label, target, met }) {
  return (
    <div className={`rounded-lg p-4 border ${met ? 'bg-green-900/20 border-green-800' : 'bg-gray-900 border-gray-800'}`}>
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="text-sm font-bold text-blue-400">{target}</div>
      <div className={`text-xs mt-1 ${met ? 'text-green-400' : 'text-gray-600'}`}>
        {met ? '✓ met' : 'pending'}
      </div>
    </div>
  )
}
