import { describe, expect, test } from "bun:test";

import {
  BANINFO_COMMAND,
  currentBan,
  formatBanInfo,
  isBanActive,
  type DiscordBanStatus,
} from "./baninfoCommand";
import type { AccountBan } from "./authDb";

const NOW = 1_700_000_000;

function ban(overrides: Partial<AccountBan> = {}): AccountBan {
  return {
    bandate: NOW - 100,
    unbandate: NOW + 100,
    bannedBy: "Discord (1)",
    reason: "cheating",
    active: true,
    ...overrides,
  };
}

const account = { id: 7, username: "alt" };

describe("isBanActive", () => {
  test("accepts permanent and unexpired temporary bans", () => {
    expect(isBanActive(ban({ bandate: NOW, unbandate: NOW }), NOW)).toBe(true);
    expect(isBanActive(ban({ unbandate: NOW + 1 }), NOW)).toBe(true);
  });

  test("rejects expired or deactivated bans", () => {
    expect(isBanActive(ban({ unbandate: NOW - 1 }), NOW)).toBe(false);
    expect(isBanActive(ban({ active: false }), NOW)).toBe(false);
  });
});

describe("currentBan", () => {
  test("returns the first active ban", () => {
    const active = ban();
    expect(currentBan([ban({ active: false }), active], NOW)).toBe(active);
  });

  test("returns null when no ban is active", () => {
    expect(currentBan([ban({ unbandate: NOW - 1 })], NOW)).toBeNull();
    expect(currentBan([], NOW)).toBeNull();
  });
});

describe("formatBanInfo", () => {
  const notBanned: DiscordBanStatus = { banned: false };

  test("shows a permanent core ban and Discord ban", () => {
    const text = formatBanInfo(
      "123",
      account,
      [ban({ bandate: NOW, unbandate: NOW, reason: "rmt" })],
      { banned: true, reason: "rmt" },
      NOW,
    );
    expect(text).toContain("**Discord:** <@123> (`123`)");
    expect(text).toContain("**Core account:** `alt` (#7)");
    expect(text).toContain("**Core ban:** Permanent");
    expect(text).toContain("**Reason:** rmt");
    expect(text).toContain("**Discord server ban:** yes — reason: rmt");
  });

  test("shows a temporary core ban with an expiry", () => {
    const text = formatBanInfo("123", account, [ban({ unbandate: NOW + 3600 })], notBanned, NOW);
    expect(text).toContain("**Core ban:** Until <t:1700003600:f>");
    expect(text).toContain("**Discord server ban:** no");
  });

  test("handles an unlinked account and unknown Discord status", () => {
    const text = formatBanInfo("123", null, [], { banned: null }, NOW);
    expect(text).toContain("**Core account:** none linked");
    expect(text).toContain("**Core ban:** n/a");
    expect(text).toContain("**Discord server ban:** unknown");
  });

  test("counts non-active ban records", () => {
    const text = formatBanInfo(
      "123",
      account,
      [ban({ active: false }), ban({ active: false })],
      notBanned,
      NOW,
    );
    expect(text).toContain("**Core ban:** none");
    expect(text).toContain("**Other ban records:** 2");
  });
});

describe("BANINFO_COMMAND", () => {
  test("is hidden by default and takes a discord_acc", () => {
    expect(BANINFO_COMMAND.name).toBe("baninfo");
    expect(BANINFO_COMMAND.default_member_permissions).toBe("0");
    expect(BANINFO_COMMAND.options?.map((option) => option.name)).toEqual(["discord_acc"]);
    const option = BANINFO_COMMAND.options?.[0] as { max_length?: number } | undefined;
    expect(option?.max_length).toBeGreaterThanOrEqual(24);
  });
});
