export default function StabilityView({ stability = {} }) {
  const {
    latency_drift_pct       = 0,
    throughput_drift_pct    = 0,
    memory_current_mb       = 0,
    memory_peak_mb          = 0,
    memory_growth_rate_mb_h = 0,
    projected_24h_growth_mb = 0,
    queue_depth_pct         = 0,
    pool_exhaustions        = 0,
    queue_rejections        = 0,
    persistence_overflows   = 0,
    is_stable               = true,
    latency_breach_hours    = -1,
    throughput_breach_hours = -1,
    memory_breach_hours     = -1,
  } = stability

  return (
    <div className="space-y-8">

      {/* Overall status */}
      <div className="flex items-center gap-3">
        <span className={`px-4 py-2 rounded-full text-sm font-semibold uppercase tracking-wider ${
          is_stable ? 'bg-green-900 text-green-300' : 'bg-red-900 text-red-300 animate-pulse'
        }`}>
          {is_stable ? 'Stable' : 'Unstable — Threshold Breached'}
        </span>
      </div>

      {/* Drift metrics */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Drift (current vs baseline)
        </h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <DriftCard
            label="Latency Drift"
            value={latency_drift_pct}
            unit="%"
            threshold={5}
            higherIsBad
          />
          <DriftCard
            label="Throughput Drift"
            value={throughput_drift_pct}
            unit="%"
            threshold={-5}
            higherIsBad={false}
          />
          <DriftCard
            label="Queue Depth"
            value={queue_depth_pct}
            unit="%"
            threshold={80}
            higherIsBad
          />
          <DriftCard
            label="Pool Exhaustions"
            value={pool_exhaustions}
            unit=""
            threshold={0}
            higherIsBad
            intValue
          />
        </div>
      </section>

      {/* Memory */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Memory (RSS)
        </h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <MemCard label="Current"   value={memory_current_mb}        unit="MB" />
          <MemCard label="Peak"      value={memory_peak_mb}            unit="MB" />
          <MemCard label="Rate"      value={memory_growth_rate_mb_h.toFixed(2)} unit="MB/hr" />
          <MemCard label="24h Proj"  value={projected_24h_growth_mb.toFixed(1)} unit="MB"
                   warn={projected_24h_growth_mb > 100} />
        </div>
      </section>

      {/* Projected breach times */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Projected Threshold Breach
        </h3>
        <div className="space-y-2">
          <BreachRow label="Memory"     hours={memory_breach_hours}     threshold="100 MB growth" />
          <BreachRow label="Latency"    hours={latency_breach_hours}    threshold="5% drift" />
          <BreachRow label="Throughput" hours={throughput_breach_hours} threshold="5% drop" />
        </div>
      </section>

      {/* Overload counters */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Overload Counters
        </h3>
        <div className="grid grid-cols-3 gap-4 max-w-lg">
          <CounterCard label="Queue Rejections"     value={queue_rejections}     />
          <CounterCard label="Persistence Overflows" value={persistence_overflows} />
          <CounterCard label="Pool Exhaustions"      value={pool_exhaustions}     />
        </div>
        <p className="text-xs text-gray-600 mt-3">
          Phase 4 overload model: all three counters must remain 0 under validated load.
          Pool exhaustions &gt; 0 is a release-blocking defect.
        </p>
      </section>

    </div>
  )
}

function DriftCard({ label, value, unit, threshold, higherIsBad, intValue = false }) {
  const breached = higherIsBad ? value > threshold : value < threshold
  const color = breached ? 'text-red-400' : Math.abs(value) > Math.abs(threshold) * 0.7 ? 'text-yellow-400' : 'text-green-400'
  const display = intValue ? value.toLocaleString() : value.toFixed(2)
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-2xl font-bold tabular-nums ${color}`}>
        {value > 0 && !intValue ? '+' : ''}{display}
        {unit && <span className="text-sm font-normal text-gray-500 ml-1">{unit}</span>}
      </div>
      <div className="text-xs text-gray-600 mt-1">
        threshold: {higherIsBad ? '<' : '>'} {threshold}{unit}
      </div>
    </div>
  )
}

function MemCard({ label, value, unit, warn = false }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-xl font-bold tabular-nums ${warn ? 'text-yellow-400' : 'text-gray-200'}`}>
        {value}
        <span className="text-sm font-normal text-gray-500 ml-1">{unit}</span>
      </div>
    </div>
  )
}

function BreachRow({ label, hours, threshold }) {
  const stable = hours < 0
  return (
    <div className="flex items-center justify-between bg-gray-900 rounded-lg px-4 py-3">
      <span className="text-sm text-gray-300 w-28">{label}</span>
      <span className="text-xs text-gray-500">{threshold}</span>
      <span className={`text-sm font-semibold tabular-nums ${
        stable ? 'text-green-400' : hours < 2 ? 'text-red-400' : hours < 6 ? 'text-yellow-400' : 'text-gray-300'
      }`}>
        {stable ? 'N/A (stable)' : `${hours.toFixed(1)} hours`}
      </span>
    </div>
  )
}

function CounterCard({ label, value }) {
  const bad = value > 0
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-lg font-bold tabular-nums ${bad ? 'text-red-400' : 'text-gray-600'}`}>
        {value.toLocaleString()}
      </div>
    </div>
  )
}
