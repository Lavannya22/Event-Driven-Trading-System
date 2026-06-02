import { useState } from 'react'
import { useWebSocket } from './hooks/useWebSocket'
import { useHistory }   from './hooks/useHistory'
import OrderBookView    from './components/OrderBookView'
import TradesView       from './components/TradesView'
import EngineView       from './components/EngineView'

const TABS = ['Order Book', 'Trades', 'Engine']

export default function App() {
  const [tab, setTab]       = useState(0)
  const { data, connected } = useWebSocket('ws://localhost:9001')

  // Rolling history for charts (one point per WebSocket tick ~200ms)
  const latencyHistory = useHistory(data, snap => ({
    t:   new Date(snap.ts).toLocaleTimeString(),
    p99: +((snap.metrics?.p99_ns  ?? 0) / 1000).toFixed(2),
    p50: +((snap.metrics?.p50_ns  ?? 0) / 1000).toFixed(2),
    t2t: +((snap.metrics?.tick_to_trade_p99_ns ?? 0) / 1000).toFixed(2),
  }))

  const throughputHistory = useHistory(data, snap => ({
    t:          new Date(snap.ts).toLocaleTimeString(),
    throughput: +((snap.metrics?.throughput_eps ?? 0) / 1000).toFixed(1),
  }))

  const tradeHistory = useHistory(data, snap => {
    const trades = snap.trades ?? []
    if (!trades.length) return null
    const last = trades[trades.length - 1]
    return { t: new Date(snap.ts).toLocaleTimeString(), price: last.price, side: last.side }
  })

  return (
    <div className="min-h-screen flex flex-col bg-gray-950 text-white">

      {/* Header */}
      <header className="flex items-center justify-between px-6 py-3 bg-gray-900 border-b border-gray-800">
        <h1 className="text-sm font-semibold tracking-widest uppercase text-gray-300">
          Trading Engine
        </h1>
        <span className={`flex items-center gap-2 text-xs ${connected ? 'text-green-400' : 'text-red-400'}`}>
          <span className={`w-1.5 h-1.5 rounded-full ${connected ? 'bg-green-400' : 'bg-red-400'}`} />
          {connected ? 'Live' : 'Disconnected'}
        </span>
      </header>

      {/* Tabs */}
      <nav className="flex bg-gray-900 border-b border-gray-800">
        {TABS.map((name, i) => (
          <button
            key={name}
            onClick={() => setTab(i)}
            className={`px-8 py-2.5 text-sm transition-colors ${
              tab === i
                ? 'border-b-2 border-blue-500 text-white'
                : 'text-gray-500 hover:text-gray-300'
            }`}
          >
            {name}
          </button>
        ))}
      </nav>

      {/* Content */}
      <main className="flex-1 p-6 overflow-auto">
        {!data ? (
          <p className="text-gray-600 text-sm">
            {connected ? 'Waiting for data…' : 'Not connected to ws://localhost:9001'}
          </p>
        ) : (
          <>
            {tab === 0 && <OrderBookView data={data.orderbook} />}
            {tab === 1 && <TradesView    trades={data.trades ?? []} history={tradeHistory} />}
            {tab === 2 && (
              <EngineView
                metrics={data.metrics ?? {}}
                replay={data.replay ?? {}}
                latencyHistory={latencyHistory}
                throughputHistory={throughputHistory}
              />
            )}
          </>
        )}
      </main>

      {/* Footer */}
      {data && (
        <footer className="px-6 py-1.5 text-xs text-gray-700 border-t border-gray-800">
          {new Date(data.ts).toISOString()}
        </footer>
      )}

    </div>
  )
}
