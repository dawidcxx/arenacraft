import type { OverlayRecord } from "@arenacraft/common";
import { InjectionToken } from "@needle-di/core";
import type { OverlayProfileRow } from "./playerOverlay";

export interface PlayerOverlayRepository {
  /** Character guid behind an overlay token, or null for an unknown token. */
  characterGuidByToken(token: string): Promise<number | null>;

  /** Name, class and SoloQ season stats; null when the character no longer exists. */
  profile(guid: number): Promise<OverlayProfileRow | null>;

  /** Account display alias (`account_alias`), as on the live ladder. */
  alias(accountId: number): Promise<string | null>;

  /** SoloQ wins/losses since midnight, server time. */
  todayRecord(guid: number): Promise<OverlayRecord>;

  /** Most recent SoloQ results, newest first; `true` is a win. */
  recentResults(guid: number, limit: number): Promise<boolean[]>;
}

export const PLAYER_OVERLAY_REPOSITORY = new InjectionToken<PlayerOverlayRepository>(
  "PlayerOverlayRepository",
);