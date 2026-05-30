import { useEffect, useRef, useState } from 'react'

export function useWebSocket(url) {
  const [data, setData]           = useState(null)
  const [connected, setConnected] = useState(false)
  const wsRef                     = useRef(null)
  const reconnectTimer            = useRef(null)

  useEffect(() => {
    let destroyed = false

    function connect() {
      if (destroyed) return
      const ws = new WebSocket(url)
      wsRef.current = ws

      ws.onopen = () => { if (!destroyed) setConnected(true) }

      ws.onmessage = (ev) => {
        if (destroyed) return
        try { setData(JSON.parse(ev.data)) } catch {}
      }

      ws.onclose = () => {
        if (destroyed) return
        setConnected(false)
        reconnectTimer.current = setTimeout(connect, 2000)
      }

      ws.onerror = () => ws.close()
    }

    connect()

    return () => {
      destroyed = true
      clearTimeout(reconnectTimer.current)
      wsRef.current?.close()
    }
  }, [url])

  return { data, connected }
}
