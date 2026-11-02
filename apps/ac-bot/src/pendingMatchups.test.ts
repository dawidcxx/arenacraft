import { describe, expect, test } from "bun:test";

import { PENDING_TTL_MS, PendingMatchups } from "./pendingMatchups";

const ROSTERS = { teamA: ["a1", "a2", "a3"], teamB: ["b1", "b2", "b3"] };

describe("PendingMatchups", () => {
  test("rosters are handed out once, when the matchup becomes ready", () => {
    const pending = new PendingMatchups();
    pending.add(7, ROSTERS);
    expect(pending.take(7)).toEqual(ROSTERS);
    expect(pending.take(7)).toBeUndefined();
  });

  test("a ready event for an unknown matchup yields nothing", () => {
    expect(new PendingMatchups().take(1)).toBeUndefined();
  });

  test("an ended matchup is forgotten", () => {
    const pending = new PendingMatchups();
    pending.add(7, ROSTERS);
    pending.forget(7);
    expect(pending.take(7)).toBeUndefined();
  });

  test("stale pops expire", () => {
    let now = 0;
    const pending = new PendingMatchups(() => now);
    pending.add(7, ROSTERS);
    now = PENDING_TTL_MS + 1;
    pending.add(8, ROSTERS);
    expect(pending.size).toBe(1);
    expect(pending.take(7)).toBeUndefined();
    expect(pending.take(8)).toEqual(ROSTERS);
  });
});
