import type { LiveLadderMessage, LiveLadderPlayer } from "@arenacraft/common";

export type LiveLadderStatus = "connecting" | "live" | "offline";

export type LiveLadderView = {
  status: LiveLadderStatus;
  players: LiveLadderPlayer[];
  updatedAt: number | null;
};

export const INITIAL_LIVE_LADDER: LiveLadderView = {
  status: "connecting",
  players: [],
  updatedAt: null,
};

export function liveLadderWebSocketUrl(): string {
  const base = import.meta.env.BASE_URL.replace(/\/$/, "");
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.host}${base}/api/live-ladder/ws`;
}

export function parseLiveLadderMessage(raw: string): LiveLadderMessage | null {
  try {
    const value = JSON.parse(raw) as LiveLadderMessage;
    if (value.type === "snapshot" && Array.isArray(value.players)) return value;
    if (value.type === "update" && Array.isArray(value.players) && typeof value.size === "number") {
      return value;
    }
    return null;
  } catch {
    return null;
  }
}

export function applyLiveLadderMessage(
  view: LiveLadderView,
  message: LiveLadderMessage,
): LiveLadderView {
  if (message.type === "snapshot") {
    return {
      ...view,
      status: "live",
      players: message.players,
      updatedAt: message.updatedAt,
    };
  }

  // Incremental update: `players` holds only the changed ranks, `size` trims the list.
  const byRank = new Map(view.players.map((player) => [player.rank, player]));
  for (const player of message.players) byRank.set(player.rank, player);

  const players = [...byRank.values()]
    .filter((player) => player.rank <= message.size)
    .sort((a, b) => a.rank - b.rank);

  return { ...view, status: "live", players, updatedAt: message.updatedAt };
}