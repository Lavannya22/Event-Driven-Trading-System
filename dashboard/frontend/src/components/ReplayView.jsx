export default function ReplayView({ data }) {
  if (!data) return <p className="text-gray-500 text-sm">No replay data.</p>

  const { source, events_replayed, last_ts, done } = data

  return (
    <div className="space-y-6 max-w-xl">
      {/* Status badge */}
      <div className="flex items-center gap-3">
        <span className={`px-3 py-1 rounded-full text-xs font-semibold uppercase tracking-wider ${
          done ? 'bg-green-900 text-green-300' : 'bg-blue-900 text-blue-300 animate-pulse'
        }`}>
          {done ? 'Complete' : 'Replaying'}
        </span>
        <span className="text-gray-400 text-sm truncate">{source || 'unknown'}</span>
      </div>

      {/* Counter */}
      <div>
        <div className="text-xs text-gray-500 uppercase tracking-wider mb-1">Events Replayed</div>
        <div className="text-4xl font-bold text-white tabular-nums">
          {events_replayed?.toLocaleString() ?? '0'}
        </div>
      </div>

      {/* Progress bar — only visible if done */}
      {done && (
        <div>
          <div className="flex justify-between text-xs text-gray-500 mb-1">
            <span>Progress</span>
            <span>100%</span>
          </div>
          <div className="h-2 bg-gray-800 rounded-full overflow-hidden">
            <div className="h-full bg-green-500 rounded-full w-full transition-all duration-500" />
          </div>
        </div>
      )}

      {/* Last event timestamp */}
      <div>
        <div className="text-xs text-gray-500 uppercase tracking-wider mb-1">Last Event Timestamp</div>
        <div className="text-lg text-gray-300 tabular-nums">{last_ts ?? '—'}</div>
      </div>
    </div>
  )
}
