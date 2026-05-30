export default function PoolsView({ pools = [], startup = {} }) {
  const {
    allocator_started       = false,
    mlockall_success        = false,
    validation_passed       = false,
    event_pool_capacity     = 0,
    order_pool_capacity     = 0,
    execution_pool_capacity = 0,
    signal_pool_capacity    = 0,
  } = startup

  return (
    <div className="space-y-8">

      {/* Startup Report */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">Startup Report</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <StatusCard label="Allocator"    ok={allocator_started}  yes="Started"   no="Not started" />
          <StatusCard label="mlockall"     ok={mlockall_success}   yes="Locked"    no="Not locked"  />
          <StatusCard label="Validation"   ok={validation_passed}  yes="Passed"    no="Failed"      />
        </div>

        <div className="mt-4 grid grid-cols-2 md:grid-cols-4 gap-4">
          <CapCard label="Event pool"     capacity={event_pool_capacity}     />
          <CapCard label="Order pool"     capacity={order_pool_capacity}     />
          <CapCard label="Execution pool" capacity={execution_pool_capacity} />
          <CapCard label="Signal pool"    capacity={signal_pool_capacity}    />
        </div>
      </section>

      {/* Live pool utilisation */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Live Pool Utilisation
        </h3>

        {pools.length === 0 ? (
          <p className="text-gray-600 text-sm">
            No pool data yet — starts after the allocator runs.
          </p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-left text-xs text-gray-500 uppercase border-b border-gray-800">
                  <th className="pb-2 pr-6">Pool</th>
                  <th className="pb-2 pr-6 text-right">Capacity</th>
                  <th className="pb-2 pr-6 text-right">Used</th>
                  <th className="pb-2 pr-6 text-right">Available</th>
                  <th className="pb-2 pr-6 text-right">Exhaustions</th>
                  <th className="pb-2">Utilisation</th>
                </tr>
              </thead>
              <tbody>
                {pools.map(p => {
                  const pct = p.capacity > 0 ? (p.used / p.capacity) * 100 : 0
                  const exhausted = p.exhaustion_count > 0
                  return (
                    <tr key={p.name} className="border-b border-gray-900">
                      <td className="py-2 pr-6 font-mono text-gray-300 capitalize">{p.name}</td>
                      <td className="py-2 pr-6 text-right tabular-nums text-gray-400">
                        {p.capacity.toLocaleString()}
                      </td>
                      <td className="py-2 pr-6 text-right tabular-nums text-blue-400">
                        {p.used.toLocaleString()}
                      </td>
                      <td className="py-2 pr-6 text-right tabular-nums text-green-400">
                        {p.available.toLocaleString()}
                      </td>
                      <td className={`py-2 pr-6 text-right tabular-nums font-semibold ${exhausted ? 'text-red-400' : 'text-gray-600'}`}>
                        {p.exhaustion_count.toLocaleString()}
                      </td>
                      <td className="py-2 w-40">
                        <div className="flex items-center gap-2">
                          <div className="flex-1 h-1.5 bg-gray-800 rounded-full overflow-hidden">
                            <div
                              className={`h-full rounded-full transition-all duration-500 ${
                                pct > 90 ? 'bg-red-500' : pct > 70 ? 'bg-yellow-500' : 'bg-blue-500'
                              }`}
                              style={{ width: `${Math.min(pct, 100)}%` }}
                            />
                          </div>
                          <span className="text-xs text-gray-500 w-10 text-right tabular-nums">
                            {pct.toFixed(1)}%
                          </span>
                        </div>
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        )}
      </section>

    </div>
  )
}

function StatusCard({ label, ok, yes, no }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-sm font-semibold ${ok ? 'text-green-400' : 'text-red-400'}`}>
        {ok ? yes : no}
      </div>
    </div>
  )
}

function CapCard({ label, capacity }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className="text-lg font-bold tabular-nums text-gray-200">
        {capacity.toLocaleString()}
      </div>
      <div className="text-xs text-gray-600 mt-1">slots</div>
    </div>
  )
}
