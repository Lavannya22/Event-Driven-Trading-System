import { useState } from 'react'
import { useWebSocket } from './hooks/useWebSocket'
import OrderBookView    from './components/OrderBookView'
import TradesView       from './components/TradesView'
import ReplayView       from './components/ReplayView'
import MetricsView      from './components/MetricsView'
import PoolsView        from './components/PoolsView'
import PerformanceView  from './components/PerformanceView'
import BenchmarkView    from './components/BenchmarkView'
import BacktestingView  from './components/BacktestingView'
import StabilityView    from './components/StabilityView'
import CIStatusView     from './components/CIStatusView'

const TABS = [
  'Order Book', 'Trades', 'Replay', 'Metrics', 'Pools', 'Performance', 'Benchmark',
  // Phase 4
  'Backtesting', 'Stability', 'CI Status',
]

// Group tabs visually
const TAB_GROUPS = {
  7: 'Phase 4',  // Backtesting starts the Phase 4 group
}

export default function App() {
  const [tab, setTab]       = useState(0)
  const { data, connected } = useWebSocket('ws://localhost:9001')

  return (
    <div className="min-h-screen flex flex-col">
      {/* Header */}
      <header className="flex items-center justify-between px-6 py-3 bg-gray-900 border-b border-gray-800">
        <h1 className="text-lg font-semibold tracking-wide text-white">
          Trading Engine Dashboard
        </h1>
        <span className={`flex items-center gap-2 text-sm ${connected ? 'text-green-400' : 'text-red-400'}`}>
          <span className={`w-2 h-2 rounded-full ${connected ? 'bg-green-400' : 'bg-red-400'}`} />
          {connected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      {/* Tab bar */}
      <nav className="flex bg-gray-900 border-b border-gray-800 overflow-x-auto">
        {TABS.map((name, i) => (
          <div key={name} className="flex items-center">
            {TAB_GROUPS[i] && (
              <span className="px-2 text-xs text-gray-600 border-l border-gray-700 ml-1 self-stretch flex items-center">
                {TAB_GROUPS[i]}
              </span>
            )}
            <button
              onClick={() => setTab(i)}
              className={`px-5 py-2 text-sm whitespace-nowrap transition-colors ${
                tab === i
                  ? 'border-b-2 border-blue-500 text-blue-400'
                  : 'text-gray-400 hover:text-gray-200'
              }`}
            >
              {name}
            </button>
          </div>
        ))}
      </nav>

      {/* Content */}
      <main className="flex-1 p-6 overflow-auto">
        {!data && (
          <p className="text-gray-500 text-sm">
            {connected ? 'Waiting for first snapshot…' : 'Not connected to ws://localhost:9001'}
          </p>
        )}
        {data && tab === 0 && <OrderBookView   data={data.orderbook} />}
        {data && tab === 1 && <TradesView       data={data.trades}    />}
        {data && tab === 2 && <ReplayView       data={data.replay}    />}
        {data && tab === 3 && <MetricsView      data={data.metrics}   />}
        {data && tab === 4 && <PoolsView        pools={data.pools ?? []} startup={data.startup ?? {}} />}
        {data && tab === 5 && <PerformanceView  metrics={data.metrics ?? {}} benchmark={data.benchmark ?? {}} />}
        {data && tab === 6 && <BenchmarkView    lastReport={data.benchmark ?? null} />}
        {/* Phase 4 tabs */}
        {data && tab === 7 && <BacktestingView  backtest={data.backtest ?? {}} />}
        {data && tab === 8 && <StabilityView    stability={data.stability ?? {}} />}
        {data && tab === 9 && <CIStatusView     ci={data.ci ?? {}} />}
      </main>

      {/* Footer */}
      {data && (
        <footer className="px-6 py-2 text-xs text-gray-600 border-t border-gray-800">
          Last snapshot: {new Date(data.ts).toISOString()}
        </footer>
      )}
    </div>
  )
}
