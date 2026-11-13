import type { LiveLadderPlayer } from "@arenacraft/common";
import { serializeError } from "serialize-error";
import { Logger } from "@/lib/Logger";
import type { LadderTopEvent, LadderTopListener, ILadderTopService } from "./ILadderTopService";

const logger = Logger.for("liveLadder");

export type PollingLadderTopOptions = {
  pollIntervalMs: number;
  fullSyncIntervalMs: number;
  now?: () => number;
};

/**
 * Polling engine shared by the real and mock services.
 *
 * - One poller for all WebSocket connections: N widgets still cost one `loadPlayers()` per tick.
 * - Polling starts with the first subscriber and stops with the last one.
 * - Every `fullSyncIntervalMs` a full snapshot is sent; other ticks send only changed rows.
 */
export abstract class PollingLadderTopService implements ILadderTopService {
  private readonly listeners = new Set<LadderTopListener>();
  private readonly now: () => number;
  private timer: ReturnType<typeof setInterval> | undefined;
  private polling = false;
  private lastPlayers: LiveLadderPlayer[] = [];
  private updatedAt = 0;
  private hasPublished = false;
  private lastFullSyncAt = 0;

  protected abstract loadPlayers(): Promise<LiveLadderPlayer[]>;

  protected constructor(private readonly options: PollingLadderTopOptions) {
    this.now = options.now ?? Date.now;
  }

  get subscriberCount(): number {
    return this.listeners.size;
  }

  subscribe(listener: LadderTopListener): () => void {
    this.listeners.add(listener);
    if (this.listeners.size === 1) this.start();

    // New widgets paint immediately instead of waiting for the next tick.
    if (this.hasPublished) {
      listener({ type: "snapshot", players: this.lastPlayers, updatedAt: this.updatedAt });
    }

    return () => {
      this.listeners.delete(listener);
      if (this.listeners.size === 0) this.stop();
    };
  }

  private start() {
    if (this.timer !== undefined) return;
    void this.poll();
    this.timer = setInterval(() => void this.poll(), this.options.pollIntervalMs);
  }

  private stop() {
    if (this.timer === undefined) return;
    clearInterval(this.timer);
    this.timer = undefined;
  }

  private async poll() {
    if (this.polling || this.listeners.size === 0) return;
    this.polling = true;
    try {
      this.publish(await this.loadPlayers());
    } catch (error) {
      logger.warn("Live ladder poll failed", { error: serializeError(error) });
    } finally {
      this.polling = false;
    }
  }

  private publish(players: LiveLadderPlayer[]) {
    const previous = this.lastPlayers;
    const now = this.now();
    this.lastPlayers = players;
    this.updatedAt = now;

    const fullSyncDue =
      !this.hasPublished || now - this.lastFullSyncAt >= this.options.fullSyncIntervalMs;

    if (fullSyncDue) {
      this.hasPublished = true;
      this.lastFullSyncAt = now;
      this.emit({ type: "snapshot", players, updatedAt: now });
      return;
    }

    const changed = changedPlayers(previous, players);
    if (changed.length === 0 && previous.length === players.length) return;

    this.emit({ type: "update", players: changed, size: players.length, updatedAt: now });
  }

  private emit(event: LadderTopEvent) {
    for (const listener of this.listeners) {
      try {
        listener(event);
      } catch (error) {
        logger.warn("Live ladder listener failed", { error: serializeError(error) });
      }
    }
  }
}

function changedPlayers(
  previous: LiveLadderPlayer[],
  next: LiveLadderPlayer[],
): LiveLadderPlayer[] {
  const changed: LiveLadderPlayer[] = [];
  for (const [index, player] of next.entries()) {
    const before = previous[index];
    if (before === undefined || !samePlayer(before, player)) changed.push(player);
  }
  return changed;
}

function samePlayer(a: LiveLadderPlayer, b: LiveLadderPlayer): boolean {
  return (
    a.rank === b.rank &&
    a.name === b.name &&
    a.class === b.class &&
    a.rating === b.rating &&
    a.wins === b.wins &&
    a.losses === b.losses
  );
}