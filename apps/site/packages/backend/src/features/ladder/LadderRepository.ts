import { InjectionToken } from "@needle-di/core";
import type { LadderRow } from "./ladder";

export interface LadderRepository {
  /** Characters that played at least one soloq game this season, best personal rating first. */
  topSoloq(limit: number): Promise<LadderRow[]>;
}

export const LADDER_REPOSITORY = new InjectionToken<LadderRepository>("LadderRepository");