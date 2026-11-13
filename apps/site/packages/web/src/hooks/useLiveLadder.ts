import { useEffect, useState } from "react";
import {
  applyLiveLadderMessage,
  INITIAL_LIVE_LADDER,
  liveLadderWebSocketUrl,
  parseLiveLadderMessage,
  type LiveLadderView,
} from "@/lib/liveLadder";

const RECONNECT_MS = 2_000;

export function useLiveLadder(): LiveLadderView {
  const [view, setView] = useState<LiveLadderView>(INITIAL_LIVE_LADDER);

  useEffect(() => {
    let socket: WebSocket | null = null;
    let reconnectTimer: number | undefined;
    let disposed = false;

    const connect = () => {
      socket = new WebSocket(liveLadderWebSocketUrl());

      socket.onopen = () => setView((current) => ({ ...current, status: "live" }));

      socket.onmessage = (event: MessageEvent<unknown>) => {
        if (typeof event.data !== "string") return;
        const message = parseLiveLadderMessage(event.data);
        if (message !== null) setView((current) => applyLiveLadderMessage(current, message));
      };

      socket.onclose = () => {
        if (disposed) return;
        setView((current) => ({ ...current, status: "offline" }));
        reconnectTimer = window.setTimeout(connect, RECONNECT_MS);
      };

      socket.onerror = () => socket?.close();
    };

    connect();

    return () => {
      disposed = true;
      window.clearTimeout(reconnectTimer);
      socket?.close();
      socket = null;
    };
  }, []);

  return view;
}