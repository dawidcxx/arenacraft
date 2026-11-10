import { describe, expect, test } from "bun:test";

import type { SeatRow } from "./authDb";
import { formatMySeat, formatSeatList, parseSeatTier } from "./seatCommand";

const seats: SeatRow[] = [
  {
    accountId: 1,
    username: "Alpha",
    discordId: "111",
    tier: "Gold",
    updatedAt: new Date("2026-11-01T00:00:00Z"),
  },
  {
    accountId: 2,
    username: "Beta",
    discordId: null,
    tier: "Gold",
    updatedAt: new Date("2026-11-05T00:00:00Z"),
  },
];

describe("parseSeatTier", () => {
  test("accepts the three tiers case-insensitively", () => {
    expect(parseSeatTier("None")).toBe("None");
    expect(parseSeatTier("gold")).toBe("Gold");
    expect(parseSeatTier("  DIAMOND  ")).toBe("Diamond");
  });

  test("rejects unknown tiers", () => {
    expect(parseSeatTier("Platinum")).toBeNull();
    expect(parseSeatTier("")).toBeNull();
  });
});

describe("formatMySeat", () => {
  test("renders the caller's tier", () => {
    expect(formatMySeat("None")).toBe("Your seat: **None**.");
    expect(formatMySeat("Diamond")).toBe("Your seat: **Diamond**.");
  });
});

describe("formatSeatList", () => {
  test("renders a header plus newest-first rows", () => {
    const lines = formatSeatList("Gold", seats).split("\n");

    expect(lines[0]).toBe("Gold seats (2)");
    expect(lines[1]).toBe("```");
    expect(lines[2]!.trim().split(/\s+/)).toEqual(["#", "Account", "Seated"]);
    expect(lines[3]!.trim().split(/\s+/)).toEqual(["1", "Beta", "2026-11-05"]);
    expect(lines[4]!.trim().split(/\s+/)).toEqual(["2", "Alpha", "2026-11-01"]);
    expect(lines[5]).toBe("```");
  });

  test("handles an empty list", () => {
    expect(formatSeatList("Diamond", [])).toBe("No players have the Diamond seat.");
  });
});
