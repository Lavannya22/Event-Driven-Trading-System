import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Legend,
} from 'recharts'

export default function EngineView({
  metrics = {}, replay = {}, latencyHistory = [], throughputHistory = [],
}) {
  const {
    throughput_eps       = 0,
    avg_latency_us       = 0,
    p50_ns = 0, p99_ns = 0, p999_ns = 0, max_ns = 0,
    tick_to_trade_p50_ns = 0,
    tick_to_trade_p99_ns = 0,
    tick_to_trade_max_ns = 0,
    signals = 0,
    noops   = 0,
  } = metrics

  const { done = false, events_replayed = 0 } = replay
  const total  = signals + noops
  const sigPct = total > 0 ? (signals / total * 100).toFixed(1) : '0.0'
  const hasHist = p99_ns > 0
  const hasT2T  = tick_to_trade_p99_ns > 0

  return (
    <div className="space-y-8 max-w-3xl">

      {/* Top row */}
      <div className="grid grid-cols-3 gap-4">
        <Stat label="Throughput" value={fmt_tp(throughput_eps)} unit="ev/s" color="text-blue-400" />
        <Stat
          label="Avg Latency" value={avg_latency_us.toFixed(3)} unit="µs"
          color={avg_latency_us < 1 ? 'text-green-400' : avg_latency_us < 5 ? 'text-yellow-400' : 'text-red-400'}
        />
        <div className="bg-gray-900 rounded-lg p-4">
          <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">Replay</div>
          <div className={`text-sm font-semibold ${done ? 'text-green-400' : 'text-blue-400'}`}>
            {done ? 'Complete' : 'Running'}
          </div>
          <div className="text-xs text-gray-600 mt-1">{events_replayed.toLocaleString()} events</div>
        </div>
      </div>

      {/* Throughput chart */}
      {throughputHistory.length > 1 && (
        <section>
          <SectionHeader title="Throughput" note="K events / sec" />
          <div className="h-44 bg-gray-900 rounded-lg p-3">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={throughputHistory} margin={{ top: 4, right: 8, bottom: 0, left: 8 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="t" tick={{ fill: '#6b7280', fontSize: 10 }} />
                <YAxis tick={{ fill: '#6b7280', fontSize: 10 }} width={40} />
                <Tooltip contentStyle={{ background: '#111827', border: '1px solid #374151', fontSize: 12 }} />
                <Line type="monotone" dataKey="throughput" stroke="#60a5fa" dot={false} strokeWidth={2} name="K ev/s" />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </section>
      )}

      {/* Engine latency */}
      <section>
        <SectionHeader
          title="Engine Latency" note="event arrival → match complete"
          ready={hasHist} pendingNote="populates after 10 000 events"
        />
        {hasHist && (
          <>
            <div className="grid grid-cols-4 gap-3 mb-4">
              <LatCard label="p50"   ns={p50_ns}  target={1000}  />
              <LatCard label="p99"   ns={p99_ns}  target={5000}  />
              <LatCard label="p99.9" ns={p999_ns} target={5000}  />
              <LatCard label="max"   ns={max_ns}  target={50000} />
            </div>
            {latencyHistory.length > 1 && (
              <div className="h-44 bg-gray-900 rounded-lg p-3">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={latencyHistory} margin={{ top: 4, right: 8, bottom: 0, left: 8 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                    <XAxis dataKey="t" tick={{ fill: '#6b7280', fontSize: 10 }} />
                    <YAxis tick={{ fill: '#6b7280', fontSize: 10 }} width={50} unit="µs" />
                    <Tooltip
                      contentStyle={{ background: '#111827', border: '1px solid #374151', fontSize: 12 }}
                      formatter={(v, name) => [`${v} µs`, name]}
                    />
                    <Legend wrapperStyle={{ fontSize: 11, color: '#9ca3af' }} />
                    <Line type="monotone" dataKey="p50" stroke="#34d399" dot={false} strokeWidth={2} name="p50" />
                    <Line type="monotone" dataKey="p99" stroke="#f59e0b" dot={false} strokeWidth={2} name="p99" />
                    <Line type="monotone" dataKey="t2t" stroke="#a78bfa" dot={false} strokeWidth={1.5} strokeDasharray="4 2" name="t2t p99" />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            )}
          </>
        )}
      </section>

      {/* Tick-to-trade */}
      <section>
        <SectionHeader
          title="Tick-to-Trade" note="event arrival → TradeExecution produced"
          ready={hasT2T} pendingNote="populates once crossing orders fill"
        />
        {hasT2T && (
          <div className="grid grid-cols-3 gap-3 max-w-sm">
            <LatCard label="p50" ns={tick_to_trade_p50_ns} target={2000}  />
            <LatCard label="p99" ns={tick_to_trade_p99_ns} target={5000}  />
            <LatCard label="max" ns={tick_to_trade_max_ns} target={50000} />
          </div>
        )}
      </section>

      {/* Order execution rate */}
      <section>
        <SectionHeader title="Order Execution" />
        <div className="flex items-center gap-6">
          <div>
            <div className="text-xs text-gray-500 uppercase mb-1">Forwarded</div>
            <div className="text-2xl font-bold text-green-400 tabular-nums">{signals.toLocaleString()}</div>
          </div>
          <div>
            <div className="text-xs text-gray-500 uppercase mb-1">Filtered</div>
            <div className="text-2xl font-bold text-gray-500 tabular-nums">{noops.toLocaleString()}</div>
          </div>
          {total > 0 && (
            <div className="flex-1 max-w-xs">
              <div className="text-xs text-gray-600 mb-1">{sigPct}% forwarded</div>
              <div className="h-1.5 bg-gray-800 rounded-full overflow-hidden">
                <div className="h-full bg-green-500 rounded-full" style={{ width: `${sigPct}%` }} />
              </div>
            </div>
          )}
        </div>
      </section>

    </div>
  )
}

function fmt_tp(eps) {
  if (eps >= 1e6) return `${(eps / 1e6).toFixed(2)}M`
  if (eps >= 1e3) return `${(eps / 1e3).toFixed(1)}K`
  return eps.toFixed(0)
}

function Stat({ label, value, unit, color }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="flex items-baseline gap-1">
        <span className={`text-2xl font-bold tabular-nums ${color}`}>{value}</span>
        <span className="text-sm text-gray-500">{unit}</span>
      </div>
    </div>
  )
}

function SectionHeader({ title, note, ready = true, pendingNote }) {
  return (
    <div className="flex items-baseline gap-2 mb-3">
      <h3 className="text-xs uppercase tracking-widest text-gray-400">{title}</h3>
      {note && <span className="text-xs text-gray-600">{note}</span>}
      {!ready && pendingNote && <span className="text-xs text-gray-700 italic">{pendingNote}</span>}
    </div>
  )
}

function LatCard({ label, ns, target }) {
  const us    = ns / 1000
  const ok    = ns > 0 && ns <= target
  const color = ns === 0        ? 'text-gray-700'
    : ok                        ? 'text-green-400'
    : ns <= target * 1.5        ? 'text-yellow-400'
    :                             'text-red-400'
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase mb-2">{label}</div>
      <div className={`text-lg font-bold tabular-nums ${color}`}>
        {ns === 0 ? '—' : us >= 1 ? `${us.toFixed(2)}µs` : `${ns}ns`}
      </div>
      <div className="text-xs text-gray-700 mt-1">
        ≤ {target >= 1000 ? `${target / 1000}µs` : `${target}ns`}
      </div>
    </div>
  )
}
