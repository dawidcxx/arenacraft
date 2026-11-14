/**
 * Twitch status is a snapshot, not a live feed: one Helix round trip per minute is plenty for a
 * streamer listing, and a page load simply reads through this cache.
 */
export const STREAMER_CACHE_TTL_MS = 60 * 1000;

/** Browsers may reuse the listing briefly; shorter than the snapshot TTL, so a reload revalidates. */
export const STREAMER_HTTP_CACHE_SECONDS = 30;

/** Helix accepts at most 100 `login` / `user_login` filters per request. */
export const TWITCH_MAX_FILTERS = 100;

/** Twitch is an external dependency; never let a hung request hold a page open. */
export const TWITCH_REQUEST_TIMEOUT_MS = 5_000;

export function chunk<T>(values: readonly T[], size: number): T[][] {
  const chunks: T[][] = [];
  for (let index = 0; index < values.length; index += size) {
    chunks.push(values.slice(index, index + size));
  }
  return chunks;
}

/** Helix returns `thumbnail_url` as a `{width}x{height}` template. */
export function thumbnailAt(template: string, width: number, height: number): string {
  return template.replace("{width}", String(width)).replace("{height}", String(height));
}
