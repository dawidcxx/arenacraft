import type { LiveLadderPlayer } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { AliasCache } from "./AliasCache";
import { LADDER_TOP_REPOSITORY, type LadderTopRepository } from "./LadderTopRepository";
import {
  LIVE_LADDER_ALIAS_TTL_MS,
  LIVE_LADDER_FULL_SYNC_INTERVAL_MS,
  LIVE_LADDER_POLL_INTERVAL_MS,
  LIVE_LADDER_SIZE,
  toLiveLadderPlayer,
} from "./liveLadder";
import { PollingLadderTopService } from "./PollingLadderTopService";

@injectable()
export class LadderTopService extends PollingLadderTopService {
  private readonly aliasCache: AliasCache;

  constructor(private readonly repository: LadderTopRepository = inject(LADDER_TOP_REPOSITORY)) {
    super({
      pollIntervalMs: LIVE_LADDER_POLL_INTERVAL_MS,
      fullSyncIntervalMs: LIVE_LADDER_FULL_SYNC_INTERVAL_MS,
    });
    this.aliasCache = new AliasCache(LIVE_LADDER_ALIAS_TTL_MS, (ids) =>
      this.repository.findAliases(ids),
    );
  }

  protected async loadPlayers(): Promise<LiveLadderPlayer[]> {
    const rows = await this.repository.topSoloq(LIVE_LADDER_SIZE);
    const aliases = await this.aliasCache.resolve(rows.map((row) => row.accountId));
    return rows.map((row, index) => toLiveLadderPlayer(row, index + 1, aliases.get(row.accountId)));
  }
}