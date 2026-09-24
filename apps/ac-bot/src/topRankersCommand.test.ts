import { describe, expect, test } from "bun:test";

import { classDisplayName, formatTopRankers } from "./topRankersCommand";
import type { Ranker } from "./topRankersCommand";

const rankers: Ranker[] = [
  { discordName: "Venruki", charName: "Athene", classId: 4, rating: 1462, seasonWins: 4, seasonGames: 4 },
  { discordName: "KALVISH", charName: "Whaazz", classId: 2, rating: 1454, seasonWins: 5, seasonGames: 7 },
];

describe("classDisplayName", () => {
  test("maps class ids to names", () => {
    expect(classDisplayName(1)).toBe("Warrior");
    expect(classDisplayName(6)).toBe("Death Knight");
    expect(classDisplayName(11)).toBe("Druid");
  });

  test("falls back for unknown ids", () => {
    expect(classDisplayName(10)).toBe("Unknown");
    expect(classDisplayName(0)).toBe("Unknown");
  });
});

describe("formatTopRankers", () => {
  test("renders a header plus one lowercased row per ranker", () => {
    const lines = formatTopRankers(rankers).split("\n");

    expect(lines[0]).toBe("SoloQ Top 10");
    expect(lines[1]).toBe("```");
    expect(lines[2]).toBe("#  Discord name  Char Name  Class    Rating  Stats");
    expect(lines[3]!.trim().split(/\s+/)).toEqual(["1", "venruki", "Athene", "Rogue", "1462", "4-0"]);
    expect(lines[4]!.trim().split(/\s+/)).toEqual(["2", "kalvish", "Whaazz", "Paladin", "1454", "5-2"]);
    expect(lines[5]).toBe("```");
  });

  test("handles an empty ladder", () => {
    expect(formatTopRankers([])).toBe("No solo queue players are ranked yet.");
  });
});
