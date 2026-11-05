import { describe, expect, test } from "bun:test";

import { getConfig } from "./config";

const base = {
  CHARACTER_DB_URL: "mysql://user:pass@db:3306/acore_characters",
};

describe("AUTH_TOKENS", () => {
  test("accepts a single token", () => {
    expect(getConfig({ ...base, AUTH_TOKENS: "single" }).AUTH_TOKENS).toEqual(["single"]);
  });

  test("splits comma-separated tokens and trims whitespace", () => {
    expect(getConfig({ ...base, AUTH_TOKENS: " one , two,, three " }).AUTH_TOKENS).toEqual([
      "one",
      "two",
      "three",
    ]);
  });

  test("requires at least one token", () => {
    expect(() => getConfig({ ...base, AUTH_TOKENS: " , " })).toThrow(/AUTH_TOKENS/);
    expect(() => getConfig(base)).toThrow(/AUTH_TOKENS/);
  });
});
