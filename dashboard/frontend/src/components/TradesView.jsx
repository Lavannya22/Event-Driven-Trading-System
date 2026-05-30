export default function TradesView({ data: trades = [] }) {
  if (!trades.length)
    return <p className="text-gray-500 text-sm">No executions yet.</p>

  // Most recent first
  const rows = [...trades].reverse()

  return (
    <div className="overflow-auto">
      <table className="w-full text-sm">
        <thead>
          <tr className="text-xs text-gray-500 uppercase tracking-wider border-b border-gray-800">
            <th className="text-left py-2 pr-4">Timestamp</th>
            <th className="text-left py-2 pr-4">Symbol</th>
            <th className="text-left py-2 pr-4">Order ID</th>
            <th className="text-right py-2 pr-4">Price</th>
            <th className="text-right py-2 pr-4">Qty</th>
            <th className="text-left py-2">Side</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((t, i) => (
            <tr key={i} className="border-b border-gray-900 hover:bg-gray-900/50 transition-colors">
              <td className="py-1.5 pr-4 text-gray-400">{t.ts}</td>
              <td className="py-1.5 pr-4 text-gray-300">{t.symbol_id}</td>
              <td className="py-1.5 pr-4 text-gray-400">{t.order_id}</td>
              <td className="py-1.5 pr-4 text-right text-white font-medium">{t.price}</td>
              <td className="py-1.5 pr-4 text-right text-gray-300">{t.qty}</td>
              <td className={`py-1.5 font-semibold ${t.side === 1 ? 'text-red-400' : 'text-green-400'}`}>
                {t.side === 1 ? 'Ask' : 'Bid'}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}
