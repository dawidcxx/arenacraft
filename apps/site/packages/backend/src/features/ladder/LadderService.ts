import type { LadderEntry } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { CONFIG, type Config } from "@/shared/Config";
import { SnapshotCache } from "@/shared/SnapshotCache";
import { toLadderEntry } from "./ladder";
import { LADDER_REPOSITORY, type LadderRepository } from "./LadderRepository";

@injectable()
export class LadderService {
  private readonly cache: SnapshotCache<LadderEntry[]>;

  constructor(
    private readonly repository: LadderRepository = inject(LADDER_REPOSITORY),
    private readonly config: Config = inject(CONFIG),
  ) {
    this.cache = new SnapshotCache(this.config.cacheTtlSeconds * 1000, () => this.load());
  }

  getSoloq(): Promise<LadderEntry[]> {
    return this.cache.get();
  }

  private async load(): Promise<LadderEntry[]> {
    return (await this.repository.topSoloq(this.config.ladderSize)).map(toLadderEntry);
  }
}