export default function OrderBookView({ data }) {
  if (!data) return <p className="text-gray-500 text-sm">No order book data.</p>

  const { best_bid, best_ask, spread, bids = [], asks = [] } = data

  return (
    <div className="space-y-4">
      {/* Summary row */}
      <div className="flex gap-8 text-sm">
        <Stat label="Best Bid" value={best_bid || '—'} color="text-green-400" />
        <Stat label="Best Ask" value={best_ask || '—'} color="text-red-400" />
        <Stat label="Spread"   value={spread ?? '—'}   color="text-yellow-400" />
      </div>

      {/* Depth table */}
      <div className="grid grid-cols-2 gap-4">
        {/* Bids */}
        <div>
          <h3 className="text-xs uppercase tracking-widest text-green-500 mb-2">Bids</h3>
          <LevelTable rows={bids} side="bid" />
        </div>

        {/* Asks */}
        <div>
          <h3 className="text-xs uppercase tracking-widest text-red-500 mb-2">Asks</h3>
          <LevelTable rows={asks} side="ask" />
        </div>
      </div>
    </div>
  )
}

function Stat({ label, value, color }) {
  return (
    <div>
      <div className="text-xs text-gray-500 uppercase tracking-wider">{label}</div>
      <div className={`text-xl font-semibold ${color}`}>{value}</div>
    </div>
  )
}

function LevelTable({ rows, side }) {
  if (!rows.length) return <p className="text-gray-600 text-sm">Empty</p>

  const color = side === 'bid' ? 'text-green-400' : 'text-red-400'
  const barColor = side === 'bid' ? 'bg-green-900' : 'bg-red-900'
  const maxQty = Math.max(...rows.map(r => r.qty), 1)

  return (
    <table className="w-full text-sm">
      <thead>
        <tr className="text-gray-500 text-xs border-b border-gray-800">
          <th className="text-left pb-1">Price</th>
          <th className="text-right pb-1">Qty</th>
        </tr>
      </thead>
      <tbody>
        {rows.map((row, i) => (
          <tr key={i} className="relative">
            {/* Depth bar */}
            <td colSpan={2} className="p-0 absolute inset-0 pointer-events-none">
              <div
                className={`h-full ${barColor} opacity-30`}
                style={{ width: `${(row.qty / maxQty) * 100}%` }}
              />
            </td>
            <td className={`py-0.5 pr-2 z-10 relative ${color}`}>{row.price}</td>
            <td className="py-0.5 text-right z-10 relative text-gray-300">{row.qty}</td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}
