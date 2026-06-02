export default function CIStatusView({ ci = {} }) {
  const {
    layer1 = {},
    layer2 = {},
    layer3 = {},
  } = ci

  return (
    <div className="space-y-8">

      {/* Layer status cards */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          CI Layer Status
        </h3>
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <LayerCard
            layer="Layer 1"
            subtitle="Correctness"
            description="Unit + integration tests — must pass before merge"
            status={layer1}
          />
          <LayerCard
            layer="Layer 2"
            subtitle="Performance"
            description="Benchmark gate — throughput + p99 latency vs baseline"
            status={layer2}
          />
          <LayerCard
            layer="Layer 3"
            subtitle="Stability"
            description="Nightly 1h + Weekly 24h — drift, memory, overload"
            status={layer3}
          />
        </div>
      </section>

      {/* Gating policy */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Gating Policy
        </h3>
        <div className="space-y-2">
          <PolicyRow
            rule="Layer 1 must pass before Layer 2 runs"
            satisfied={layer1.state === 'passing'}
          />
          <PolicyRow
            rule="Layer 2 must pass before Layer 3 nightly runs"
            satisfied={layer2.state === 'passing'}
          />
          <PolicyRow
            rule="Layer 3 nightly must pass before Layer 3 weekly runs"
            satisfied={layer3.state === 'passing'}
          />
          <PolicyRow
            rule="Stability regressions are release-blocking defects"
            satisfied={layer3.state !== 'failing'}
            alwaysWarn
          />
        </div>
      </section>

      {/* Failure details */}
      {(layer1.last_failure || layer2.last_failure || layer3.last_failure) && (
        <section>
          <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
            Last Failure Reason
          </h3>
          <div className="space-y-3">
            {layer1.last_failure && (
              <FailureDetail layer="Layer 1" message={layer1.last_failure} />
            )}
            {layer2.last_failure && (
              <FailureDetail layer="Layer 2" message={layer2.last_failure} />
            )}
            {layer3.last_failure && (
              <FailureDetail layer="Layer 3" message={layer3.last_failure} />
            )}
          </div>
        </section>
      )}

      <p className="text-xs text-gray-600">
        Layer 3 CI requires bare-metal Linux with isolcpus, nohz_full, rcu_nocbs,
        performance governor, THP disabled, swap disabled.
        WSL2, Docker Desktop, Cloud VMs, and shared CI runners are forbidden.
      </p>

    </div>
  )
}

function LayerCard({ layer, subtitle, description, status = {} }) {
  const state = status.state ?? 'unknown'
  const colors = {
    passing: { badge: 'bg-green-900 text-green-300', dot: 'bg-green-400' },
    failing: { badge: 'bg-red-900 text-red-300',     dot: 'bg-red-400'   },
    unknown: { badge: 'bg-gray-800 text-gray-400',   dot: 'bg-gray-500'  },
  }
  const c = colors[state] ?? colors.unknown

  return (
    <div className="bg-gray-900 rounded-lg p-5 space-y-3">
      <div className="flex items-center justify-between">
        <div>
          <div className="font-semibold text-white">{layer}</div>
          <div className="text-xs text-gray-500 mt-0.5">{subtitle}</div>
        </div>
        <span className={`flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-semibold uppercase ${c.badge}`}>
          <span className={`w-1.5 h-1.5 rounded-full ${c.dot}`} />
          {state}
        </span>
      </div>
      <p className="text-xs text-gray-600">{description}</p>
      {status.last_run && (
        <div className="text-xs text-gray-500">Last run: {status.last_run}</div>
      )}
    </div>
  )
}

function PolicyRow({ rule, satisfied, alwaysWarn = false }) {
  const color = alwaysWarn && !satisfied
    ? 'text-red-400'
    : satisfied
      ? 'text-green-400'
      : 'text-yellow-400'
  const icon = satisfied ? '✓' : '✗'
  return (
    <div className="flex items-center gap-3 bg-gray-900 rounded-lg px-4 py-2.5">
      <span className={`text-sm font-bold ${color}`}>{icon}</span>
      <span className="text-sm text-gray-300">{rule}</span>
    </div>
  )
}

function FailureDetail({ layer, message }) {
  return (
    <div className="bg-red-900/20 border border-red-800 rounded-lg px-4 py-3">
      <div className="text-xs text-red-400 font-semibold uppercase mb-1">{layer}</div>
      <div className="text-sm text-gray-300 font-mono">{message}</div>
    </div>
  )
}
