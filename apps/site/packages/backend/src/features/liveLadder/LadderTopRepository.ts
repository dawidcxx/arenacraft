import { InjectionToken } from "@needle-di/core";
import type { LiveLadderRow } from "./liveLadder";

export interface LadderTopRepository {
  /** SoloQ characters with a personal rating, best first. */
  topSoloq(limit: number): Promise<LiveLadderRow[]>;

  /** Account aliases for the given accounts (empty when the auth DB is not configured). */
  findAliases(accountIds: number[]): Promise<Map<number, string>>;
}

export const LADDER_TOP_REPOSITORY = new InjectionToken<LadderTopRepository>("LadderTopRepository");