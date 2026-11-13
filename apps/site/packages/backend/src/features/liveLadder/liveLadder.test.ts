import { describe, expect, test } from "bun:test";
import type { LiveLadderPlayer } from "@arenacraft/common";
import { AliasCache } from "./AliasCache";
import type { LadderTopEvent } from "./ILadderTopService";
import { PollingLadderTopService } from "./PollingLadderTopService";

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

function player(rank: number, name: string, rating: number): LiveLadderPlayer {
  return { rank, name, class: "Mage", rating, wins: 10, losses: 2 };
}

class TestService extends PollingLadderTopService {
  calls = 0;
  players: LiveLadderPlayer[] = [player(1, "A", 2000), player(2, "B", 1900)];

  constructor(pollMs = 10, fullSyncMs = 60_000) {
    super({ pollIntervalMs: pollMs, fullSyncIntervalMs: fullSyncMs });
  }

  protected async loadPlayers(): Promise<LiveLadderPlayer[]> {
    this.calls++;
    return this.players;
  }
}

async function waitFor(events: LadderTopEvent[], type: LadderTopEvent["type"], timeout = 500) {
  const started = Date.now();
  while (Date.now() - started < timeout) {
    const found = events.find((event) => event.type === type);
    if (found !== undefined) return found;
    await sleep(2);
  }
  throw new Error(`Timed out waiting for a ${type} event`);
}

describe("PollingLadderTopService", () => {
  test("sends a snapshot as soon as the first widget connects", async () => {
    const service = new TestService();
    const events: LadderTopEvent[] = [];
    const unsubscribe = service.subscribe((event) => events.push(event));

    const snapshot = await waitFor(events, "snapshot");
    expect(snapshot.players).toHaveLength(2);
    unsubscribe();
  });

  test("one poller feeds every subscriber", async () => {
    const service = new TestService(10);
    const unsubscribeAll = [
      service.subscribe(() => {}),
      service.subscribe(() => {}),
      service.subscribe(() => {}),
    ];

    await sleep(35);
    // ~3 ticks in 35ms. Three independent pollers would be ~9-12.
    expect(service.calls).toBeLessThanOrEqual(6);

    for (const unsubscribe of unsubscribeAll) unsubscribe();
  });

  test("stops polling when the last subscriber leaves", async () => {
    const service = new TestService(10);
    const unsubscribe = service.subscribe(() => {});
    await sleep(25);

    unsubscribe();
    const callsAfterUnsubscribe = service.calls;
    await sleep(40);
    expect(service.calls).toBe(callsAfterUnsubscribe);
  });

  test("sends only changed ranks between full syncs", async () => {
    const service = new TestService(10);
    const events: LadderTopEvent[] = [];
    const unsubscribe = service.subscribe((event) => events.push(event));

    await waitFor(events, "snapshot");
    service.players = [player(1, "A", 2100), player(2, "B", 1900)];

    const update = await waitFor(events, "update");
    expect(update.type).toBe("update");
    if (update.type !== "update") throw new Error("unreachable");
    expect(update.players.map((p) => p.rank)).toEqual([1]);
    expect(update.players[0]?.rating).toBe(2100);
    expect(update.size).toBe(2);

    unsubscribe();
  });

  test("re-sends the full state on the full sync interval", async () => {
    const service = new TestService(5, 12);
    const events: LadderTopEvent[] = [];
    const unsubscribe = service.subscribe((event) => events.push(event));

    await waitFor(events, "snapshot");
    await sleep(30);

    const snapshots = events.filter((event) => event.type === "snapshot");
    expect(snapshots.length).toBeGreaterThanOrEqual(2);

    unsubscribe();
  });

  test("a late subscriber gets the cached snapshot immediately", async () => {
    const service = new TestService(10);
    const unsubscribe = service.subscribe(() => {});
    await sleep(15);

    const late: LadderTopEvent[] = [];
    const unsubscribeLate = service.subscribe((event) => late.push(event));
    expect(late).toHaveLength(1);
    expect(late[0]?.type).toBe("snapshot");

    unsubscribe();
    unsubscribeLate();
  });
});

describe("AliasCache", () => {
  test("reloads an account only after the TTL", async () => {
    let now = 0;
    let loads = 0;
    const cache = new AliasCache(
      1000,
      async (ids) => {
        loads++;
        return new Map(ids.map((id) => [id, `alias-${id}`]));
      },
      () => now,
    );

    expect(await cache.resolve([1, 2])).toEqual(
      new Map([
        [1, "alias-1"],
        [2, "alias-2"],
      ]),
    );
    now = 999;
    expect(await cache.resolve([1, 2])).toEqual(
      new Map([
        [1, "alias-1"],
        [2, "alias-2"],
      ]),
    );
    expect(loads).toBe(1);

    now = 1000;
    await cache.resolve([1, 2]);
    expect(loads).toBe(2);
  });

  test("remembers accounts without an alias so they are not re-queried", async () => {
    let loads = 0;
    const cache = new AliasCache(1000, async () => {
      loads++;
      return new Map<number, string>();
    });

    expect((await cache.resolve([7])).size).toBe(0);
    expect((await cache.resolve([7])).size).toBe(0);
    expect(loads).toBe(1);
  });

  test("only asks for missing ids", async () => {
    const requested: number[][] = [];
    const cache = new AliasCache(1000, async (ids) => {
      requested.push(ids);
      return new Map<number, string>();
    });

    await cache.resolve([1, 2]);
    await cache.resolve([2, 3]);
    expect(requested).toEqual([[1, 2], [3]]);
  });

  test("keeps the cached alias when the refresh fails", async () => {
    let fail = false;
    const cache = new AliasCache(0, async () => {
      if (fail) throw new Error("auth db down");
      return new Map([[1, "Kalvax"]]);
    });

    expect(await cache.resolve([1])).toEqual(new Map([[1, "Kalvax"]]));
    fail = true;
    expect(await cache.resolve([1])).toEqual(new Map([[1, "Kalvax"]]));
  });
});