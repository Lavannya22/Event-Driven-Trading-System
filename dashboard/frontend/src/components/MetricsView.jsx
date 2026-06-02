export default function MetricsView({ data }) {
  if (!data) return <p className="text-gray-500 text-sm">No metrics data.</p>

  const {
    throughput_eps          = 0,
    queue_occupancy         = 0,
    avg_latency_us          = 0,
    p50_ns                  = 0,
    p99_ns                  = 0,
    p999_ns                 = 0,
    max_ns                  = 0,
    tick_to_trade_p50_ns    = 0,
    tick_to_trade_p99_ns    = 0,
    tick_to_trade_max_ns    = 0,
    signals                 = 0,
    noops                   = 0,
  } = data

  const total      = signals + noops
  const signalPct  = total > 0 ? ((signals / total) * 100).toFixed(1) : '0.0'
  const hasLiveHist = p99_ns > 0
  const hasT2T      = tick_to_trade_p99_ns > 0

  return (
    <div className="space-y-8">

      {/* Throughput + queue */}
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
          label="Avg Engine Latency"
          value={avg_latency_us.toFixed(3)}
          unit="µs"
          color={avg_latency_us < 1 ? 'text-green-400'
               : avg_latency_us < 5 ? 'text-yellow-400'
               : 'text-red-400'}
        />
      </div>

      {/* Live engine latency histogram */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Engine Latency — Live Histogram
          <span className="ml-2 text-gray-600 normal-case">(event arrival → match complete, includes strategy)</span>
        </h3>
        {hasLiveHist ? (
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <LatCard label="p50"   ns={p50_ns}   target={1000} />
            <LatCard label="p99"   ns={p99_ns}   target={5000} />
            <LatCard label="p99.9" ns={p999_ns}  target={5000} />
            <LatCard label="max"   ns={max_ns}   target={50000} />
          </div>
        ) : (
          <p className="text-gray-600 text-sm">
            Populates after 10 000 events are processed.
          </p>
        )}
      </section>

      {/* Tick-to-trade */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Tick-to-Trade Latency
          <span className="ml-2 text-gray-600 normal-case">(event arrival → TradeExecution produced)</span>
        </h3>
        {hasT2T ? (
          <div className="grid grid-cols-3 gap-4 max-w-lg">
            <LatCard label="p50"   ns={tick_to_trade_p50_ns} target={2000}  />
            <LatCard label="p99"   ns={tick_to_trade_p99_ns} target={5000}  />
            <LatCard label="max"   ns={tick_to_trade_max_ns} target={50000} />
          </div>
        ) : (
          <p className="text-gray-600 text-sm">
            Populates once crossing orders produce fills.
          </p>
        )}
      </section>

      {/* Order execution summary */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Order Execution
          <span className="ml-2 text-gray-600 normal-case">(see Trades tab for fill list)</span>
        </h3>
        <div className="grid grid-cols-2 gap-4 max-w-sm">
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 uppercase mb-1">Signals forwarded</div>
            <div className="text-2xl font-bold text-green-400 tabular-nums">
              {signals.toLocaleString()}
            </div>
            <div className="text-xs text-gray-600 mt-1">{signalPct}% of events</div>
          </div>
          <div className="bg-gray-900 rounded-lg p-4">
            <div className="text-xs text-gray-500 uppercase mb-1">Noops dropped</div>
            <div className="text-2xl font-bold text-gray-400 tabular-nums">
              {noops.toLocaleString()}
            </div>
            <div className="text-xs text-gray-600 mt-1">
              {(100 - parseFloat(signalPct)).toFixed(1)}% of events
            </div>
          </div>
        </div>
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
      </section>

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

function LatCard({ label, ns, target }) {
  const us = ns / 1000
  const ok = ns > 0 && ns <= target
  const color = ns === 0 ? 'text-gray-600'
    : ok ? 'text-green-400'
    : ns <= target * 2 ? 'text-yellow-400'
    : 'text-red-400'
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase mb-2">{label}</div>
      <div className={`text-xl font-bold tabular-nums ${color}`}>
        {ns === 0 ? '—' : us >= 1 ? `${us.toFixed(2)} µs` : `${ns} ns`}
      </div>
      <div className="text-xs text-gray-600 mt-1">
        target ≤ {target >= 1000 ? `${target/1000}µs` : `${target}ns`}
      </div>
    </div>
  )
}
