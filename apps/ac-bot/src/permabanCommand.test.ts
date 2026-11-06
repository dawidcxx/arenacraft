import { describe, expect, test } from "bun:test";

import { buildPermaBanModal, formatPermaBanReply, isPermaBanConfirmed, PERMABAN_COMMAND } from "./permabanCommand";

describe("isPermaBanConfirmed", () => {
  test("requires the confirmation word, case-insensitively", () => {
    expect(isPermaBanConfirmed("BAN")).toBe(true);
    expect(isPermaBanConfirmed(" ban ")).toBe(true);
  });

  test("rejects anything else", () => {
    expect(isPermaBanConfirmed("")).toBe(false);
    expect(isPermaBanConfirmed("BANN")).toBe(false);
    expect(isPermaBanConfirmed("yes")).toBe(false);
  });
});

describe("buildPermaBanModal", () => {
  test("shows the target/reason and requires BAN", () => {
    const modal = buildPermaBanModal("permaban-confirm:test", "123", "cheating").toJSON();
    expect(modal.custom_id).toBe("permaban-confirm:test");
    expect(modal.title).toBe("Confirm permanent ban");

    const json = JSON.stringify(modal);
    expect(json).toContain("cheating");
    expect(json).toContain("Type BAN to confirm");
    expect(json).toContain('"custom_id":"confirm"');
  });
});

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
