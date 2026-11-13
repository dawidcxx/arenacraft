import type { LiveLadderPlayer } from "@arenacraft/common";
import { classNameOf } from "@/features/ladder/ladder";

/** Players shown in the OBS widget. */
export const LIVE_LADDER_SIZE = 5;

/** One DB round trip per second, shared by every connected widget. */
export const LIVE_LADDER_POLL_INTERVAL_MS = 1_000;

/** A periodic full state keeps clients that missed an update from drifting. */
export const LIVE_LADDER_FULL_SYNC_INTERVAL_MS = 10_000;

/** Alias rows are cheap but rarely change; 1 minute matches `.alias get` in the core. */
export const LIVE_LADDER_ALIAS_TTL_MS = 60_000;

export interface LiveLadderRow {
  charName: string;
  classId: number;
  accountId: number;
  rating: number;
  seasonWins: number;
  seasonGames: number;
}

export function toLiveLadderPlayer(
  row: LiveLadderRow,
  rank: number,
  alias: string | undefined,
): LiveLadderPlayer {
  return {
    rank,
    name: alias ?? row.charName,
    class: classNameOf(row.classId),
    rating: row.rating,
    wins: row.seasonWins,
    losses: Math.max(0, row.seasonGames - row.seasonWins),
  };
}