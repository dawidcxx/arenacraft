import { describe, expect, test } from "bun:test";

import { formatOnlineList } from "./onlineListCommand";
import type { OnlineCharacter } from "./characterDb";

const online: OnlineCharacter[] = [
  { charName: "Athene", classId: 4 },
  { charName: "Whaazz", classId: 2 },
];

describe("formatOnlineList", () => {
  test("renders a count plus one name/class line per character", () => {
    const lines = formatOnlineList(online).split("\n");

    expect(lines[0]).toBe("Online now (2)");
    expect(lines[1]).toBe("```");
    expect(lines[2]).toBe("Athene (Rogue)");
    expect(lines[3]).toBe("Whaazz (Paladin)");
    expect(lines[4]).toBe("```");
  });

  test("handles an empty list", () => {
    expect(formatOnlineList([])).toBe("No players are online right now.");
  });
});
