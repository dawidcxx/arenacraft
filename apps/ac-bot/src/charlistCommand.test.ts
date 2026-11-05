import { describe, expect, test } from "bun:test";

import { findMemberByName, formatCharList, parseMemberQuery } from "./charlistCommand";
import type { CharacterSummary } from "./characterDb";

const members = [
  { id: "111", user: { username: "Flutters", globalName: "Flutter" }, displayName: "Flutters" },
  { id: "222", user: { username: "killah", globalName: null }, displayName: "Killah" },
];

describe("parseMemberQuery", () => {
  test("strips a leading @ and lowercases names", () => {
    expect(parseMemberQuery("@SomePlayer")).toEqual({ id: null, name: "someplayer" });
    expect(parseMemberQuery("  Venruki  ")).toEqual({ id: null, name: "venruki" });
  });

  test("extracts ids from mentions and raw snowflakes", () => {
    expect(parseMemberQuery("<@111>")).toEqual({ id: "111", name: null });
    expect(parseMemberQuery("<@!222>")).toEqual({ id: "222", name: null });
    expect(parseMemberQuery("333")).toEqual({ id: "333", name: null });
  });

  test("returns an empty query for blank input", () => {
    expect(parseMemberQuery("   ")).toEqual({ id: null, name: null });
    expect(parseMemberQuery("@")).toEqual({ id: null, name: null });
  });
});

describe("findMemberByName", () => {
  test("matches username, global name, or display name case-insensitively", () => {
    expect(findMemberByName(members, "@flutters")?.id).toBe("111");
    expect(findMemberByName(members, "flutter")?.id).toBe("111");
    expect(findMemberByName(members, "KILLAH")?.id).toBe("222");
  });

  test("matches mentions and raw ids", () => {
    expect(findMemberByName(members, "<@!222>")?.id).toBe("222");
    expect(findMemberByName(members, "111")?.id).toBe("111");
  });

  test("returns undefined when nobody matches", () => {
    expect(findMemberByName(members, "athene")).toBeUndefined();
    expect(findMemberByName([], "flutters")).toBeUndefined();
  });
});

describe("formatCharList", () => {
  const chars: CharacterSummary[] = [
    { charName: "Flutters", classId: 3 },
    { charName: "Killah", classId: 6 },
  ];

  test("renders one name/class bullet per character", () => {
    expect(formatCharList("Flutter", chars)).toBe("- Flutters, Hunter\n- Killah, Death Knight");
  });

  test("handles a member with no characters", () => {
    expect(formatCharList("Flutter", [])).toBe("**Flutter** has no characters.");
  });
});
