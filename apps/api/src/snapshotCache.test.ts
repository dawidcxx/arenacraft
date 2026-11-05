import { describe, expect, test } from "bun:test";

import { SnapshotCache } from "./snapshotCache";

describe("SnapshotCache", () => {
  test("reuses the value inside the ttl", async () => {
    let now = 0;
    let loads = 0;
    const cache = new SnapshotCache(1000, async () => ++loads, () => now);

    expect(await cache.get()).toBe(1);
    now = 999;
    expect(await cache.get()).toBe(1);
    now = 1000;
    expect(await cache.get()).toBe(2);
  });

  test("concurrent misses share one load", async () => {
    let loads = 0;
    let release!: () => void;
    const gate = new Promise<void>((resolve) => (release = resolve));
    const cache = new SnapshotCache(1000, async () => {
      loads++;
      await gate;
      return "ladder";
    });

    const all = Promise.all(Array.from({ length: 50 }, () => cache.get()));
    release();
    expect(await all).toEqual(Array(50).fill("ladder"));
    expect(loads).toBe(1);
  });

  test("serves the last good value when a refresh fails", async () => {
    let now = 0;
    let fail = false;
    const cache = new SnapshotCache(
      1000,
      async () => {
        if (fail) throw new Error("db down");
        return "good";
      },
      () => now,
    );

    expect(await cache.get()).toBe("good");
    fail = true;
    now = 5000;
    expect(await cache.get()).toBe("good");
  });

  test("propagates the error when there is nothing cached", async () => {
    const cache = new SnapshotCache(1000, async () => {
      throw new Error("db down");
    });
    await expect(cache.get()).rejects.toThrow("db down");
  });
});
