import { describe, expect, test } from "bun:test";

import { classDisplayName, formatTopRankers } from "./topRankersCommand";
import type { Ranker } from "./topRankersCommand";

const rankers: Ranker[] = [
  { discordName: "venruki", charName: "Athene", classId: 4, rating: 1462, seasonWins: 4, seasonGames: 4 },
  { discordName: "kalvish", charName: "Whaazz", classId: 2, rating: 1454, seasonWins: 5, seasonGames: 7 },
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
  test("renders a box-drawing table with a row per ranker", () => {
    expect(formatTopRankers(rankers)).toBe(
      [
        "┌───┬─────────┬───────────┬─────────┬────────┬───────┐",
        "│ # │ Discord │ Character │ Class   │ Rating │ Stats │",
        "├───┼─────────┼───────────┼─────────┼────────┼───────┤",
        "│ 1 │ venruki │ Athene    │ Rogue   │   1462 │ 4-0   │",
        "│ 2 │ kalvish │ Whaazz    │ Paladin │   1454 │ 5-2   │",
        "└───┴─────────┴───────────┴─────────┴────────┴───────┘",
      ].join("\n"),
    );
  });

  test("handles an empty ladder", () => {
    expect(formatTopRankers([])).toBe("");
  });
});
