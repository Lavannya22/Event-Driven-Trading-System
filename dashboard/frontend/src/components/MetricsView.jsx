export default function MetricsView({ data }) {
  if (!data) return <p className="text-gray-500 text-sm">No metrics data.</p>

  const {
    throughput_eps  = 0,
    queue_occupancy = 0,
    avg_latency_us  = 0,
    signals         = 0,
    noops           = 0,
  } = data

  const total = signals + noops
  const signalPct = total > 0 ? ((signals / total) * 100).toFixed(1) : '0.0'

  return (
    <div className="space-y-8">
      {/* Top-row cards */}
      <div className="grid grid-cols-3 gap-4">
        <MetricCard
          label="Throughput"
          value={throughput_eps >= 1000
            ? `${(throughput_eps / 1000).toFixed(1)}k`
            : throughput_eps.toFixed(0)}
          unit="events/s"
          color="text-blue-400"
        />
        <MetricCard
          label="Queue Occupancy"
          value={`${(queue_occupancy * 100).toFixed(1)}`}
          unit="%"
          color={queue_occupancy > 0.8 ? 'text-red-400' : 'text-yellow-400'}
        />
        <MetricCard
          label="Avg Latency"
          value={avg_latency_us.toFixed(2)}
          unit="μs"
          color="text-purple-400"
        />
      </div>

      {/* Strategy breakdown */}
      <div>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">Strategy Output</h3>
        <div className="grid grid-cols-2 gap-4 max-w-sm">
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 uppercase mb-1">Signals</div>
            <div className="text-2xl font-bold text-green-400 tabular-nums">
              {signals.toLocaleString()}
            </div>
            <div className="text-xs text-gray-600 mt-1">{signalPct}% of total</div>
          </div>
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 uppercase mb-1">Noops</div>
            <div className="text-2xl font-bold text-gray-400 tabular-nums">
              {noops.toLocaleString()}
            </div>
            <div className="text-xs text-gray-600 mt-1">{(100 - parseFloat(signalPct)).toFixed(1)}% of total</div>
          </div>
        </div>

        {/* Signal ratio bar */}
        {total > 0 && (
          <div className="mt-3 max-w-sm">
            <div className="h-1.5 bg-gray-800 rounded-full overflow-hidden">
              <div
                className="h-full bg-green-500 rounded-full transition-all duration-500"
                style={{ width: `${signalPct}%` }}
              />
            </div>
          </div>
        )}
      </div>

      {/* Phase 1 timing disclaimer */}
      <p className="text-xs text-gray-600">
        Phase 1: latency measured with std::chrono — not suitable as authoritative latency data.
        RDTSCP measurement begins in Phase 2.
      </p>
    </div>
  )
}

function MetricCard({ label, value, unit, color }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="flex items-baseline gap-1">
        <span className={`text-3xl font-bold tabular-nums ${color}`}>{value}</span>
        <span className="text-sm text-gray-500">{unit}</span>
      </div>
    </div>
  )
}
