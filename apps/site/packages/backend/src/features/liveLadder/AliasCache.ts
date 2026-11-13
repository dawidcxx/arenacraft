type AliasEntry = {
  alias: string | null;
  expiresAt: number;
};

/**
 * Per-account alias cache with a TTL. The top-5 query stays hot every second; aliases are only
 * re-read once a minute (or when a new account enters the top 5).
 */
export class AliasCache {
  private readonly entries = new Map<number, AliasEntry>();

  constructor(
    private readonly ttlMs: number,
    private readonly load: (accountIds: number[]) => Promise<Map<number, string>>,
    private readonly now: () => number = Date.now,
  ) {}

  async resolve(accountIds: number[]): Promise<Map<number, string>> {
    const now = this.now();
    const missing = [...new Set(accountIds)].filter((id) => {
      const entry = this.entries.get(id);
      return entry === undefined || entry.expiresAt <= now;
    });

    if (missing.length > 0) {
      try {
        const loaded = await this.load(missing);
        for (const id of missing) {
          // Cache misses too, otherwise every poll re-queries accounts without an alias.
          this.entries.set(id, {
            alias: loaded.get(id) ?? null,
            expiresAt: this.now() + this.ttlMs,
          });
        }
        this.prune();
      } catch {
        // Keep serving whatever is cached; the next poll retries.
      }
    }

    const aliases = new Map<number, string>();
    for (const id of accountIds) {
      const alias = this.entries.get(id)?.alias;
      if (alias !== null && alias !== undefined) aliases.set(id, alias);
    }
    return aliases;
  }

  /** Top-5 rotations accumulate slowly; drop expired accounts before the map grows forever. */
  private prune() {
    if (this.entries.size <= 500) return;
    const now = this.now();
    for (const [id, entry] of this.entries) {
      if (entry.expiresAt <= now) this.entries.delete(id);
    }
  }
}