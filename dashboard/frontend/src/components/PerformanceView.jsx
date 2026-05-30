export default function PerformanceView({ metrics = {}, benchmark = {} }) {
  const {
    throughput_eps  = 0,
    avg_latency_us  = 0,
    signals         = 0,
    noops           = 0,
  } = metrics

  // Phase 3 histogram percentiles from backend (nanoseconds)
  const {
    p50_ns   = null,
    p95_ns   = null,
    p99_ns   = null,
    p999_ns  = null,
    max_ns   = null,
  } = benchmark

  const hasHistogram = p50_ns !== null

  return (
    <div className="space-y-8">

      {/* Throughput + live latency */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">Live Engine Metrics</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <MetricCard
            label="Throughput"
            value={throughput_eps >= 1e6
              ? `${(throughput_eps / 1e6).toFixed(2)}M`
              : throughput_eps >= 1e3
                ? `${(throughput_eps / 1e3).toFixed(1)}K`
                : throughput_eps.toFixed(0)}
            unit="events/s"
            color={throughput_eps >= 10e6 ? 'text-green-400' : 'text-blue-400'}
          />
          <MetricCard
            label="Avg Latency"
            value={avg_latency_us.toFixed(2)}
            unit="µs"
            color={avg_latency_us < 1 ? 'text-green-400' : avg_latency_us < 5 ? 'text-yellow-400' : 'text-red-400'}
          />
          <MetricCard label="Signals" value={signals.toLocaleString()} unit="" color="text-green-400" />
          <MetricCard label="Noops"   value={noops.toLocaleString()}   unit="" color="text-gray-400" />
        </div>
      </section>

      {/* Latency histogram percentiles */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Latency Percentiles {hasHistogram ? '(ns)' : '— run benchmark to populate'}
        </h3>
        {hasHistogram ? (
          <div className="grid grid-cols-2 md:grid-cols-5 gap-4">
            <PercentileCard label="p50"   value={p50_ns}  target={1000}  />
            <PercentileCard label="p95"   value={p95_ns}  target={2000}  />
            <PercentileCard label="p99"   value={p99_ns}  target={5000}  />
            <PercentileCard label="p99.9" value={p999_ns} target={5000}  />
            <PercentileCard label="max"   value={max_ns}  target={50000} />
          </div>
        ) : (
          <div className="bg-gray-900 rounded-lg p-6 text-center">
            <p className="text-gray-500 text-sm">
              Run <code className="text-blue-400">./run_benchmark small</code> to generate latency percentiles.
            </p>
            <p className="text-gray-600 text-xs mt-2">Target: p99 &lt; 5µs · p99.9 &lt; 5µs</p>
          </div>
        )}
      </section>

      {/* Phase 3 targets reference */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">Phase 3 Targets</h3>
        <div className="grid grid-cols-3 gap-4 max-w-lg">
          <TargetRow label="Throughput" target="≥ 10M events/s" />
          <TargetRow label="p99"        target="&lt; 5µs"       />
          <TargetRow label="Cache miss" target="&lt; 1%"        />
        </div>
        <p className="text-xs text-gray-600 mt-4">
          Note: WSL2 results are informational only. Authoritative measurements require bare-metal Linux
          with performance governor, THP disabled, and isolated CPUs.
        </p>
      </section>

    </div>
  )
}

function MetricCard({ label, value, unit, color }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="flex items-baseline gap-1">
        <span className={`text-2xl font-bold tabular-nums ${color}`}>{value}</span>
        {unit && <span className="text-sm text-gray-500">{unit}</span>}
      </div>
    </div>
  )
}

function PercentileCard({ label, value, target }) {
  const ok = value <= target
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase mb-2">{label}</div>
      <div className={`text-xl font-bold tabular-nums ${ok ? 'text-green-400' : 'text-red-400'}`}>
        {value?.toLocaleString()}
        <span className="text-xs font-normal text-gray-500 ml-1">ns</span>
      </div>
      <div className="text-xs text-gray-600 mt-1">target ≤ {target.toLocaleString()}ns</div>
    </div>
  )
}

function TargetRow({ label, target }) {
  return (
    <div className="bg-gray-900 rounded-lg p-3">
      <div className="text-xs text-gray-500 mb-1">{label}</div>
      <div className="text-sm font-semibold text-blue-400">{target}</div>
    </div>
  )
}
