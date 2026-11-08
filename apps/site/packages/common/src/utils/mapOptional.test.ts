import { describe, expect, it } from "bun:test";
import { mapOptional } from "./mapOptional";

describe("mapOptional", () => {
  it("returns null for nullish input", () => {
    expect(mapOptional(null, (x: number) => x * 2)).toBe(null);
    expect(mapOptional(undefined, (x: number) => x * 2)).toBe(null);
  });

  it("applies mapping fn for non-nullish input", () => {
    expect(mapOptional(5, (x) => x * 2)).toBe(10);
  });

  it("handles string transformation", () => {
    expect(mapOptional("hello", (s) => s.toUpperCase())).toBe("HELLO");
  });

  it("handles falsy non-null values", () => {
    expect(mapOptional(0, (x) => x + 1)).toBe(1);
    expect(mapOptional("", (s) => s.length)).toBe(0);
    expect(mapOptional(false, (b) => !b)).toBe(true);
  });
});