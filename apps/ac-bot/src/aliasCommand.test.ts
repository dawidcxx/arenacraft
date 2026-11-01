import { describe, expect, test } from "bun:test";

import { hasVipRole, normalizeAlias } from "./aliasCommand";

describe("normalizeAlias", () => {
  test("accepts identifier-safe aliases", () => {
    expect(normalizeAlias("Dir")).toBe("Dir");
    expect(normalizeAlias("  kasia_95  ")).toBe("kasia_95");
    expect(normalizeAlias("Zolw-01")).toBe("Zolw-01");
    expect(normalizeAlias("Żółw")).toBe("Żółw");
  });

  test("rejects whitespace, punctuation and overlong values", () => {
    expect(normalizeAlias("")).toBeNull();
    expect(normalizeAlias("Dir the Great")).toBeNull();
    expect(normalizeAlias("a=b")).toBeNull();
    expect(normalizeAlias("a|b")).toBeNull();
    expect(normalizeAlias("x".repeat(33))).toBeNull();
  });
});

describe("hasVipRole", () => {
  test("matches the role case-insensitively", () => {
    expect(hasVipRole([{ name: "Member" }, { name: "VIP" }], "vip")).toBe(true);
    expect(hasVipRole([{ name: "member" }], "vip")).toBe(false);
    expect(hasVipRole([], "vip")).toBe(false);
  });
});
