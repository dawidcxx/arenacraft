import { describe, expect, test } from "bun:test";
import {
  LIVE_LADDER_MAX_CONNECTIONS,
  WebSocketConnectionLimiter,
} from "./WebSocketConnectionLimiter";

describe("WebSocketConnectionLimiter", () => {
  test("allows up to the limit and rejects the next connection", () => {
    const limiter = new WebSocketConnectionLimiter();
    const releases: Array<() => void> = [];

    for (let i = 0; i < LIVE_LADDER_MAX_CONNECTIONS; i++) {
      const release = limiter.tryAcquire();
      expect(release).not.toBeNull();
      releases.push(release!);
    }

    expect(limiter.count).toBe(LIVE_LADDER_MAX_CONNECTIONS);
    expect(limiter.tryAcquire()).toBeNull();

    releases[0]?.();
    expect(limiter.count).toBe(LIVE_LADDER_MAX_CONNECTIONS - 1);
    const reclaimed = limiter.tryAcquire();
    expect(reclaimed).not.toBeNull();

    for (const release of releases.slice(1)) release();
    reclaimed?.();
    expect(limiter.count).toBe(0);
  });

  test("releasing twice does not free a second slot", () => {
    const limiter = new WebSocketConnectionLimiter();
    const release = limiter.tryAcquire();
    expect(limiter.count).toBe(1);

    release?.();
    release?.();
    expect(limiter.count).toBe(0);
  });

  test("all slots can be reclaimed", () => {
    const limiter = new WebSocketConnectionLimiter();
    const releases = Array.from({ length: LIVE_LADDER_MAX_CONNECTIONS }, () =>
      limiter.tryAcquire(),
    );
    expect(limiter.tryAcquire()).toBeNull();

    for (const release of releases) release?.();
    expect(limiter.count).toBe(0);
    expect(limiter.tryAcquire()).not.toBeNull();
  });
});