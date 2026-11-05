import { describe, expect, test } from "bun:test";

import { roleOf, toLadderEntry } from "./ladder";

describe("roleOf", () => {
  test("uses the stored soloq role", () => {
    expect(roleOf(2, 0)).toBe("Melee");
    expect(roleOf(5, 1)).toBe("Caster");
    expect(roleOf(11, 2)).toBe("Healer");
  });

  test("falls back to the class role for single-role classes", () => {
    expect(roleOf(4, null)).toBe("Melee");
    expect(roleOf(9, null)).toBe("Caster");
  });

  test("leaves hybrids without a stored role empty", () => {
    expect(roleOf(7, null)).toBe("");
    expect(roleOf(7, 99)).toBe("");
  });
});

describe("toLadderEntry", () => {
  test("maps a row to the website shape", () => {
    expect(
      toLadderEntry({ charName: "Kalvax", classId: 4, storedRole: null, rating: 2412, seasonWins: 184, seasonGames: 245 }),
    ).toEqual({ name: "Kalvax", class: "Rogue", role: "Melee", rating: 2412, wins: 184, losses: 61 });
  });

  test("never reports negative losses", () => {
    expect(
      toLadderEntry({ charName: "X", classId: 6, storedRole: 0, rating: 1500, seasonWins: 3, seasonGames: 2 }).losses,
    ).toBe(0);
  });

  test("unknown class ids stay readable", () => {
    expect(toLadderEntry({ charName: "X", classId: 42, storedRole: null, rating: 1, seasonWins: 0, seasonGames: 1 }).class).toBe(
      "Unknown",
    );
  });
});
