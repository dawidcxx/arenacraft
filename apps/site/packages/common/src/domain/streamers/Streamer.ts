import { InferOutput, nullable, number, object, string } from "valibot";

// One affiliated Twitch channel on the streamer zone (`/streamers`), live or offline.
export const StreamerLive = object({
  title: string(),
  game: string(),
  viewers: number(),
  // Unix epoch milliseconds, so the page can render how long the channel has been live.
  startedAt: number(),
  // Twitch preview image sized for a card; null when Twitch returns none.
  thumbnailUrl: nullable(string()),
});

export type StreamerLive = InferOutput<typeof StreamerLive>;

export const Streamer = object({
  // Lowercase Twitch login (the part after twitch.tv/); the stable key of the channel.
  login: string(),
  // Twitch display name, falling back to the login when Twitch has no profile for it.
  name: string(),
  avatarUrl: nullable(string()),
  // Null while the channel is offline.
  live: nullable(StreamerLive),
});

export type Streamer = InferOutput<typeof Streamer>;

/** Live channels whose title mentions the realm are promoted above the rest. */
export const FEATURED_TITLE_KEYWORD = "arenacraft";

export function isFeaturedStreamer(streamer: Streamer): boolean {
  return streamer.live?.title.toLowerCase().includes(FEATURED_TITLE_KEYWORD) ?? false;
}

/**
 * Featured live channels first, then the other live channels by viewer count, then the offline
 * channels by name. The backend sorts once and the website renders the array in the given order.
 */
export function sortStreamers(streamers: readonly Streamer[]): Streamer[] {
  return [...streamers].sort((a, b) => {
    const rankDelta = rank(a) - rank(b);
    if (rankDelta !== 0) return rankDelta;
    if (a.live !== null && b.live !== null && a.live.viewers !== b.live.viewers) {
      return b.live.viewers - a.live.viewers;
    }
    return a.name.localeCompare(b.name);
  });
}

function rank(streamer: Streamer): number {
  if (streamer.live === null) return 2;
  return isFeaturedStreamer(streamer) ? 0 : 1;
}
