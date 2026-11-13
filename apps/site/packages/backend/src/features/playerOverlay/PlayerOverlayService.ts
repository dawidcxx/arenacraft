import type { PlayerOverlay } from "@arenacraft/common";
import { inject, injectable } from "@needle-di/core";
import { SnapshotCache } from "@/shared/SnapshotCache";
import { PlayerOverlayNotFoundError } from "./error/playerOverlayErrors";
import {
  PLAYER_OVERLAY_CACHE_TTL_MS,
  PLAYER_OVERLAY_LAST_MATCHES,
  PLAYER_OVERLAY_MAX_CACHED_TOKENS,
  toPlayerOverlay,
} from "./playerOverlay";
import { PLAYER_OVERLAY_REPOSITORY, type PlayerOverlayRepository } from "./PlayerOverlayRepository";

@injectable()
export class PlayerOverlayService {
  // One snapshot per token, so a streamer with several browser sources (or a refresh storm) still
  // costs one set of queries per TTL. Insertion order doubles as a cheap LRU for the size cap.
  private readonly caches = new Map<string, SnapshotCache<PlayerOverlay>>();

  constructor(
    private readonly repository: PlayerOverlayRepository = inject(PLAYER_OVERLAY_REPOSITORY),
    private readonly now: () => number = Date.now,
  ) {}

  get(token: string): Promise<PlayerOverlay> {
    let cache = this.caches.get(token);
    if (cache === undefined) {
      cache = new SnapshotCache(PLAYER_OVERLAY_CACHE_TTL_MS, () => this.load(token), this.now);
      if (this.caches.size >= PLAYER_OVERLAY_MAX_CACHED_TOKENS) {
        const oldest = this.caches.keys().next().value;
        if (oldest !== undefined) this.caches.delete(oldest);
      }
    } else {
      this.caches.delete(token);
    }
    this.caches.set(token, cache);

    return cache.get().catch((error: unknown) => {
      // Do not keep a cache slot for a token that does not resolve.
      if (error instanceof PlayerOverlayNotFoundError) this.caches.delete(token);
      throw error;
    });
  }

  private async load(token: string): Promise<PlayerOverlay> {
    const guid = await this.repository.characterGuidByToken(token);
    if (guid === null) throw new PlayerOverlayNotFoundError();

    const profile = await this.repository.profile(guid);
    if (profile === null) throw new PlayerOverlayNotFoundError();

    const [alias, today, recent] = await Promise.all([
      this.repository.alias(profile.accountId),
      this.repository.todayRecord(guid),
      this.repository.recentResults(guid, PLAYER_OVERLAY_LAST_MATCHES),
    ]);

    return toPlayerOverlay(profile, alias, today, recent, this.now());
  }
}