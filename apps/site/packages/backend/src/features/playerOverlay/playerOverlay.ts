import {
  PLAYER_OVERLAY_LAST_MATCHES,
  type OverlayRecord,
  type PlayerOverlay,
} from "@arenacraft/common";
import { classNameOf } from "@/features/ladder/ladder";

/** Each overlay polls every few seconds; one DB round trip per token per window is plenty. */
export const PLAYER_OVERLAY_CACHE_TTL_MS = 5_000;

/** Upper bound on per-token caches kept in memory (one per streamer with OBS open). */
export const PLAYER_OVERLAY_MAX_CACHED_TOKENS = 500;

export { PLAYER_OVERLAY_LAST_MATCHES };

export interface OverlayProfileRow {
  charName: string;
  classId: number;
  accountId: number;
  /** Null when the character has no SoloQ team yet. */
  rating: number | null;
  seasonWins: number;
  seasonGames: number;
}

export function toPlayerOverlay(
  profile: OverlayProfileRow,
  alias: string | null,
  today: OverlayRecord,
  recentNewestFirst: boolean[],
  updatedAt: number,
): PlayerOverlay {
  return {
    name: alias ?? profile.charName,
    class: classNameOf(profile.classId),
    rating: profile.rating,
    all: {
      wins: profile.seasonWins,
      losses: Math.max(0, profile.seasonGames - profile.seasonWins),
    },
    today,
    lastMatches: recentNewestFirst
      .slice(0, PLAYER_OVERLAY_LAST_MATCHES)
      .reverse()
      .map((won) => (won ? "W" : "L")),
    updatedAt,
  };
}