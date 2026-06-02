import {
  AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
} from 'recharts'

export default function OrderBookView({ data }) {
  if (!data) return <p className="text-gray-600 text-sm">No order book data.</p>

  const { best_bid = 0, best_ask = 0, spread = 0, bids = [], asks = [] } = data
  const depthData = buildDepth(bids, asks)

  return (
    <div className="space-y-6 max-w-2xl">

      <div className="flex gap-8">
        <Top label="Best Bid" value={best_bid || '—'} color="text-green-400" />
        <Top label="Best Ask" value={best_ask || '—'} color="text-red-400" />
        <Top label="Spread"   value={spread ?? '—'}   color="text-gray-400" />
      </div>

      {depthData.length > 1 && (
        <section>
          <div className="text-xs uppercase tracking-widest text-gray-400 mb-3">Market Depth</div>
          <div className="h-52 bg-gray-900 rounded-lg p-3">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={depthData} margin={{ top: 4, right: 8, bottom: 0, left: 8 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="price" tick={{ fill: '#6b7280', fontSize: 10 }} />
                <YAxis tick={{ fill: '#6b7280', fontSize: 10 }} width={40} />
                <Tooltip
                  contentStyle={{ background: '#111827', border: '1px solid #374151', fontSize: 12 }}
                  labelFormatter={v => `Price ${v}`}
                />
                <Area
                  type="stepAfter" dataKey="cumBid" stroke="#22c55e" fill="#22c55e"
                  fillOpacity={0.15} dot={false} name="Bid depth"
                />
                <Area
                  type="stepBefore" dataKey="cumAsk" stroke="#f87171" fill="#f87171"
                  fillOpacity={0.15} dot={false} name="Ask depth"
                />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </section>
      )}

      <div className="grid grid-cols-2 gap-6">
        <DepthSide title="Bids" rows={bids} side="bid" />
        <DepthSide title="Asks" rows={asks} side="ask" />
      </div>

    </div>
  )
}

function buildDepth(bids, asks) {
  const map = {}

  let cumBid = 0
  ;[...bids].sort((a, b) => b.price - a.price).forEach(r => {
    cumBid += r.qty
    map[r.price] = { price: r.price, cumBid }
  })

  let cumAsk = 0
  ;[...asks].sort((a, b) => a.price - b.price).forEach(r => {
    cumAsk += r.qty
    map[r.price] = { ...(map[r.price] ?? { price: r.price }), cumAsk }
  })

  return Object.values(map).sort((a, b) => a.price - b.price)
}

function Top({ label, value, color }) {
  return (
    <div>
      <div className="text-xs text-gray-600 uppercase tracking-wider mb-1">{label}</div>
      <div className={`text-2xl font-bold tabular-nums ${color}`}>{value}</div>
    </div>
  )
}

function DepthSide({ title, rows, side }) {
  const color = side === 'bid' ? 'text-green-400' : 'text-red-400'
  return (
    <div>
      <div className={`text-xs uppercase tracking-widest mb-2 ${color}`}>{title}</div>
      {rows.length === 0 ? (
        <p className="text-gray-700 text-sm">Empty</p>
      ) : (
        <table className="w-full text-sm">
          <thead>
            <tr className="text-xs text-gray-600 border-b border-gray-800">
              <th className="text-left pb-1">Price</th>
              <th className="text-right pb-1">Qty</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((r, i) => (
              <tr key={i}>
                <td className={`py-0.5 ${color} tabular-nums`}>{r.price}</td>
                <td className="py-0.5 text-right text-gray-300 tabular-nums">{r.qty}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  )
}
