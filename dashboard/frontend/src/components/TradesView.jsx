import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
} from 'recharts'

export default function TradesView({ trades = [], history = [] }) {
  if (!trades.length)
    return <p className="text-gray-600 text-sm">No executions yet.</p>

  return (
    <div className="space-y-6 max-w-2xl">

      {history.length > 1 && (
        <section>
          <div className="text-xs uppercase tracking-widest text-gray-400 mb-3">Fill Price History</div>
          <div className="h-52 bg-gray-900 rounded-lg p-3">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={history} margin={{ top: 4, right: 8, bottom: 0, left: 8 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="t" tick={{ fill: '#6b7280', fontSize: 10 }} />
                <YAxis tick={{ fill: '#6b7280', fontSize: 10 }} domain={['auto', 'auto']} width={50} />
                <Tooltip
                  contentStyle={{ background: '#111827', border: '1px solid #374151', fontSize: 12 }}
                />
                <Line
                  type="monotone" dataKey="price" stroke="#60a5fa" dot={false}
                  strokeWidth={2} name="Fill price"
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </section>
      )}

      <table className="w-full text-sm">
        <thead>
          <tr className="text-xs text-gray-600 uppercase border-b border-gray-800">
            <th className="text-left py-2 pr-6">Side</th>
            <th className="text-right py-2 pr-6">Price</th>
            <th className="text-right py-2 pr-6">Qty</th>
            <th className="text-right py-2">Order ID</th>
          </tr>
        </thead>
        <tbody>
          {[...trades].reverse().map((t, i) => (
            <tr key={i} className="border-b border-gray-900">
              <td className={`py-1.5 pr-6 font-semibold ${t.side === 1 ? 'text-red-400' : 'text-green-400'}`}>
                {t.side === 1 ? 'Sell' : 'Buy'}
              </td>
              <td className="py-1.5 pr-6 text-right tabular-nums text-white font-medium">
                {t.price}
              </td>
              <td className="py-1.5 pr-6 text-right tabular-nums text-gray-300">
                {t.qty}
              </td>
              <td className="py-1.5 text-right tabular-nums text-gray-600 text-xs">
                {t.order_id}
              </td>
            </tr>
          ))}
        </tbody>
      </table>

    </div>
  )
}
