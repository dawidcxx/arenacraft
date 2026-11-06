import { describe, expect, test } from "bun:test";

import { normalizeDiscordId } from "./discordId";

describe("normalizeDiscordId", () => {
  test("accepts raw snowflakes and mentions", () => {
    expect(normalizeDiscordId("123456789012345678")).toBe("123456789012345678");
    expect(normalizeDiscordId("  <@123456789012345678>  ")).toBe("123456789012345678");
    expect(normalizeDiscordId("<@!123456789012345678>")).toBe("123456789012345678");
  });

  test("rejects empty, non-numeric and overlong values", () => {
    expect(normalizeDiscordId("")).toBeNull();
    expect(normalizeDiscordId("   ")).toBeNull();
    expect(normalizeDiscordId("not-an-id")).toBeNull();
    expect(normalizeDiscordId("@123")).toBeNull();
    expect(normalizeDiscordId("1".repeat(21))).toBeNull();
  });
});
