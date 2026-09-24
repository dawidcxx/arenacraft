import { describe, expect, test } from "bun:test";

import { TtlCache } from "./ttlCache";

describe("TtlCache", () => {
  test("caches a value for the ttl window", async () => {
    let now = 1_000;
    let loads = 0;
    const cache = new TtlCache(30_000, async () => ++loads, () => now);

    expect(await cache.get()).toBe(1);
    now += 29_999;
    expect(await cache.get()).toBe(1);
    expect(loads).toBe(1);
  });

  test("reloads once the window expires", async () => {
    let now = 1_000;
    let loads = 0;
    const cache = new TtlCache(30_000, async () => ++loads, () => now);

    expect(await cache.get()).toBe(1);
    now += 30_000;
    expect(await cache.get()).toBe(2);
    expect(loads).toBe(2);
  });

  test("shares one in-flight load between concurrent callers", async () => {
    let loads = 0;
    let resolve!: (value: number) => void;
    const cache = new TtlCache(
      30_000,
      () =>
        new Promise<number>((r) => {
          loads += 1;
          resolve = r;
        }),
    );

    const first = cache.get();
    const second = cache.get();
    resolve(7);

    expect(await first).toBe(7);
    expect(await second).toBe(7);
    expect(loads).toBe(1);
  });

  test("does not cache a failed load", async () => {
    let loads = 0;
    const cache = new TtlCache(30_000, async () => {
      loads += 1;
      if (loads === 1) throw new Error("boom");
      return loads;
    });

    await expect(cache.get()).rejects.toThrow("boom");
    expect(await cache.get()).toBe(2);
  });
});
