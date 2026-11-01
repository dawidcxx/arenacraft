import { describe, expect, test } from "bun:test";

import { findMemberByUsername, formatLookup } from "./lookupCommand";

const members = [
  { user: { username: "Venruki" }, id: "111" },
  { user: { username: "KALVISH" }, id: "222" },
];

describe("findMemberByUsername", () => {
  test("matches the Discord username case-insensitively", () => {
    expect(findMemberByUsername(members, "venruki")?.id).toBe("111");
    expect(findMemberByUsername(members, "kalvish")?.id).toBe("222");
  });

  test("returns undefined when nobody matches", () => {
    expect(findMemberByUsername(members, "athene")).toBeUndefined();
    expect(findMemberByUsername([], "venruki")).toBeUndefined();
  });
});

describe("formatLookup", () => {
  test("renders a mention and raw id when the member is found", () => {
    expect(formatLookup("Athene", "venruki", "111")).toBe("**Athene** is played by <@111> (`111`).");
  });

  test("falls back to the account name when no member matches", () => {
    expect(formatLookup("Athene", "venruki", null)).toBe(
      "**Athene** is on account `venruki`, but no matching Discord member was found.",
    );
  });
});
