import { useState, useEffect } from 'react'

// Keeps a rolling buffer of the last `maxPoints` snapshots.
// `selector` extracts the data point from each incoming snapshot.
// Keyed on `snapshot.ts` so one point is added per broadcast cycle.
export function useHistory(snapshot, selector, maxPoints = 80) {
  const [history, setHistory] = useState([])

  useEffect(() => {
    if (!snapshot) return
    const point = selector(snapshot)
    if (point == null) return
    setHistory(prev => {
      const next = [...prev, point]
      return next.length > maxPoints ? next.slice(-maxPoints) : next
    })
  }, [snapshot?.ts])   // one point per WebSocket tick

  return history
}
