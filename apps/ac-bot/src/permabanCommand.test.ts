import { describe, expect, test } from "bun:test";

import { formatPermaBanReply, PERMABAN_COMMAND } from "./permabanCommand";

describe("formatPermaBanReply", () => {
  test("mentions the banned user and the core account", () => {
    expect(formatPermaBanReply("123", "core account `alt` (#7)")).toBe(
      "Permanently banned <@123> from Discord. Core: core account `alt` (#7).",
    );
  });

  test("handles an unlinked Discord user", () => {
    expect(formatPermaBanReply("123", "no linked core account")).toBe(
      "Permanently banned <@123> from Discord. Core: no linked core account.",
    );
  });
});

describe("PERMABAN_COMMAND", () => {
  test("is hidden by default and requires an id and reason", () => {
    expect(PERMABAN_COMMAND.name).toBe("permaban");
    expect(PERMABAN_COMMAND.default_member_permissions).toBe("0");
    expect(PERMABAN_COMMAND.options?.map((option) => option.name)).toEqual(["discord_id", "reason"]);
    // Mentions such as <@!123456789012345678> are 24 characters.
    const discordIdOption = PERMABAN_COMMAND.options?.[0] as { max_length?: number } | undefined;
    expect(discordIdOption?.max_length).toBeGreaterThanOrEqual(24);
  });
});
