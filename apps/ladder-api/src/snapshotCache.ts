// Caches one value for `ttlMs`. Concurrent misses share a single load (no thundering herd on the
// database), and when a refresh fails the last good value keeps being served.
export class SnapshotCache<T> {
  private value: T | undefined;
  private loadedAt = 0;
  private pending: Promise<T> | undefined;

  constructor(
    private readonly ttlMs: number,
    private readonly load: () => Promise<T>,
    private readonly now: () => number = Date.now,
  ) {}

  async get(): Promise<T> {
    if (this.value !== undefined && this.now() - this.loadedAt < this.ttlMs) return this.value;

    if (!this.pending) {
      this.pending = this.load()
        .then((value) => {
          this.value = value;
          this.loadedAt = this.now();
          return value;
        })
        .finally(() => {
          this.pending = undefined;
        });
    }

    try {
      return await this.pending;
    } catch (error) {
      if (this.value !== undefined) return this.value; // stale beats down
      throw error;
    }
  }
}
