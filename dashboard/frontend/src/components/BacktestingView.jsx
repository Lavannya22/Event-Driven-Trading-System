export default function BacktestingView({ backtest = {} }) {
  const {
    results = [],
    running = false,
    current_run = 0,
    total_runs = 0,
  } = backtest

  return (
    <div className="space-y-8">

      {/* Status bar */}
      <div className="flex items-center gap-4">
        <span className={`px-3 py-1 rounded-full text-xs font-semibold uppercase tracking-wider ${
          running
            ? 'bg-blue-900 text-blue-300 animate-pulse'
            : results.length > 0
              ? 'bg-green-900 text-green-300'
              : 'bg-gray-800 text-gray-400'
        }`}>
          {running ? `Run ${current_run} / ${total_runs}` : results.length > 0 ? 'Complete' : 'Idle'}
        </span>
        {total_runs > 0 && (
          <span className="text-sm text-gray-400">
            {results.length} of {total_runs} runs recorded
          </span>
        )}
      </div>

      {/* Results table */}
      <section>
        <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
          Historical Run Results
        </h3>

        {results.length === 0 ? (
          <div className="bg-gray-900 rounded-lg p-6 text-center">
            <p className="text-gray-500 text-sm">
              No backtest results yet. Start a backtest run to populate this view.
            </p>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-gray-500 uppercase border-b border-gray-800">
                  <th className="text-left pb-2 pr-4">Run</th>
                  <th className="text-right pb-2 pr-4">Window</th>
                  <th className="text-right pb-2 pr-4">Threshold</th>
                  <th className="text-right pb-2 pr-4">Trades</th>
                  <th className="text-right pb-2 pr-4">PnL</th>
                  <th className="text-right pb-2 pr-4">Sharpe</th>
                  <th className="text-right pb-2 pr-4">Win %</th>
                  <th className="text-right pb-2">Max DD</th>
                </tr>
              </thead>
              <tbody>
                {results.map((r, i) => {
                  const pnlColor = r.pnl > 0 ? 'text-green-400' : r.pnl < 0 ? 'text-red-400' : 'text-gray-300'
                  const sharpeColor = r.sharpe > 1 ? 'text-green-400' : r.sharpe > 0 ? 'text-yellow-400' : 'text-gray-400'
                  return (
                    <tr key={i} className="border-b border-gray-900 hover:bg-gray-900/50">
                      <td className="py-1.5 pr-4 text-gray-300 font-mono">#{r.run_number}</td>
                      <td className="py-1.5 pr-4 text-right tabular-nums text-gray-400">{r.window_size}</td>
                      <td className="py-1.5 pr-4 text-right tabular-nums text-gray-400">{r.threshold?.toFixed(3)}</td>
                      <td className="py-1.5 pr-4 text-right tabular-nums text-gray-300">{r.total_trades?.toLocaleString()}</td>
                      <td className={`py-1.5 pr-4 text-right tabular-nums font-semibold ${pnlColor}`}>
                        {r.pnl != null ? (r.pnl >= 0 ? '+' : '') + r.pnl.toFixed(0) : '—'}
                      </td>
                      <td className={`py-1.5 pr-4 text-right tabular-nums ${sharpeColor}`}>
                        {r.sharpe?.toFixed(3) ?? '—'}
                      </td>
                      <td className="py-1.5 pr-4 text-right tabular-nums text-gray-300">
                        {r.win_rate != null ? (r.win_rate * 100).toFixed(1) + '%' : '—'}
                      </td>
                      <td className="py-1.5 text-right tabular-nums text-red-400">
                        {r.max_drawdown != null ? r.max_drawdown.toFixed(0) : '—'}
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        )}
      </section>

      {/* Parameter sweep summary */}
      {results.length > 1 && (
        <section>
          <h3 className="text-xs uppercase tracking-widest text-gray-500 mb-3">
            Parameter Sweep Summary
          </h3>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <SweepCard
              label="Best PnL"
              value={Math.max(...results.map(r => r.pnl ?? -Infinity)).toFixed(0)}
              color="text-green-400"
            />
            <SweepCard
              label="Best Sharpe"
              value={Math.max(...results.map(r => r.sharpe ?? -Infinity)).toFixed(3)}
              color="text-blue-400"
            />
            <SweepCard
              label="Best Win Rate"
              value={(Math.max(...results.map(r => r.win_rate ?? 0)) * 100).toFixed(1) + '%'}
              color="text-yellow-400"
            />
            <SweepCard
              label="Min Drawdown"
              value={Math.min(...results.map(r => r.max_drawdown ?? Infinity)).toFixed(0)}
              color="text-purple-400"
            />
          </div>
        </section>
      )}

    </div>
  )
}

function SweepCard({ label, value, color }) {
  return (
    <div className="bg-gray-900 rounded-lg p-4">
      <div className="text-xs text-gray-500 uppercase tracking-wider mb-2">{label}</div>
      <div className={`text-xl font-bold tabular-nums ${color}`}>{value}</div>
    </div>
  )
}
