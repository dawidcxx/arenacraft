import { describe, expect, test } from "bun:test";

import { normalizeAccountName, WOW_NAME_PATTERN } from "./wowName";

describe("normalizeAccountName", () => {
  test("uppercases valid alphanumeric names", () => {
    expect(normalizeAccountName("john99")).toBe("JOHN99");
    expect(normalizeAccountName("Alpha")).toBe("ALPHA");
  });

  test("rejects names that are not supportable as Wow accounts", () => {
    expect(normalizeAccountName("john.doe")).toBeNull();
    expect(normalizeAccountName("john_doe")).toBeNull();
    expect(normalizeAccountName("john-doe")).toBeNull();
    expect(normalizeAccountName("with space")).toBeNull();
    expect(normalizeAccountName("j")).toBeNull();
    expect(normalizeAccountName("a".repeat(21))).toBeNull();
  });

  test("exposes the pattern", () => {
    expect(WOW_NAME_PATTERN.test("Abc123")).toBe(true);
    expect(WOW_NAME_PATTERN.test("bad.name")).toBe(false);
  });
});
