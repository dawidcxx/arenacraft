import type { Streamer } from "@arenacraft/common";

export type StreamersState =
  | { status: "loading" }
  | { status: "error"; message: string }
  | { status: "ready"; streamers: Streamer[] };

export const INITIAL_STREAMERS: StreamersState = { status: "loading" };

export function onlineStreamers(streamers: readonly Streamer[]): Streamer[] {
  return streamers.filter((streamer) => streamer.live !== null);
}

export function offlineStreamers(streamers: readonly Streamer[]): Streamer[] {
  return streamers.filter((streamer) => streamer.live === null);
}

export function watchUrl(login: string): string {
  return `https://www.twitch.tv/${encodeURIComponent(login)}`;
}

/** `812` / `1.2k` / `12k` — compact enough for a badge. */
export function formatViewers(count: number): string {
  if (count < 1_000) return String(count);
  const thousands = count / 1_000;
  return `${thousands >= 10 ? Math.round(thousands) : thousands.toFixed(1)}k`;
}

/** `47m` / `2h` / `2h 13m` — coarse on purpose, the list only refreshes every few minutes. */
export function formatUptime(startedAt: number, now: number): string {
  const minutes = Math.max(0, Math.floor((now - startedAt) / 60_000));
  const hours = Math.floor(minutes / 60);
  const rest = minutes % 60;
  if (hours === 0) return `${rest}m`;
  return rest === 0 ? `${hours}h` : `${hours}h ${rest}m`;
}

/** The fetch client reports failures as `Request failed with <status>: <body>`. */
export function streamerErrorMessage(error: unknown): string {
  if (error instanceof Error && error.message.startsWith("Request failed with 503")) {
    return "Streamer status is unavailable right now. Try again in a few minutes.";
  }
  return "Could not load the streamer list. Try again in a moment.";
}
