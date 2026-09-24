/**
 * In-process TTL cache for an async loader. Concurrent callers during a miss
 * share the same in-flight promise, so a burst of commands issues one load.
 */
export class TtlCache<T> {
  private entry?: { value: T; expiresAt: number };
  private inflight?: Promise<T>;

  constructor(
    private readonly ttlMs: number,
    private readonly load: () => Promise<T>,
    private readonly now: () => number = Date.now,
  ) {}

  async get(): Promise<T> {
    if (this.entry && this.now() < this.entry.expiresAt) return this.entry.value;
    if (this.inflight) return this.inflight;

    this.inflight = this.load()
      .then((value) => {
        this.entry = { value, expiresAt: this.now() + this.ttlMs };
        return value;
      })
      .finally(() => {
        this.inflight = undefined;
      });

    return this.inflight;
  }

  clear(): void {
    this.entry = undefined;
  }
}
