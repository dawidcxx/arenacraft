// Matchups that popped but whose players are not all inside the arena yet. The bot keeps the two rosters here
// and only books the voice rooms once the core reports the arena ready (anti-snipe: a pop alone moves nobody).

export interface PendingRosters {
  teamA: string[];
  teamB: string[];
}

interface Entry extends PendingRosters {
  addedAt: number;
}

// A pop that never becomes ready (declined invites, cancelled arena, missed event) is forgotten after this.
export const PENDING_TTL_MS = 10 * 60_000;

export class PendingMatchups {
  private readonly entries = new Map<number, Entry>();

  constructor(private readonly now: () => number = Date.now) {}

  add(instanceId: number, rosters: PendingRosters): void {
    this.prune();
    this.entries.set(instanceId, { ...rosters, addedAt: this.now() });
  }

  /** Removes and returns the rosters of a matchup that just became ready. */
  take(instanceId: number): PendingRosters | undefined {
    this.prune();
    const entry = this.entries.get(instanceId);
    if (!entry) return undefined;
    this.entries.delete(instanceId);
    return { teamA: entry.teamA, teamB: entry.teamB };
  }

  /** Drops a matchup that ended (or was aborted) before it became ready. */
  forget(instanceId: number): void {
    this.entries.delete(instanceId);
  }

  get size(): number {
    return this.entries.size;
  }

  private prune(): void {
    const cutoff = this.now() - PENDING_TTL_MS;
    for (const [instanceId, entry] of this.entries) if (entry.addedAt < cutoff) this.entries.delete(instanceId);
  }
}
