import { describe, expect, test } from "bun:test";

import { formatRegisterReply } from "./registerCommand";

describe("formatRegisterReply", () => {
  test("fresh registration includes the realmlist hint", () => {
    const reply = formatRegisterReply("alice", "hunter2", "Welcome to Arenacraft! Your login:", true);

    expect(reply).toBe(
      [
        "Welcome to Arenacraft! Your login:",
        "",
        "**Username**  => `alice`",
        "**Password**  => `hunter2`",
        "",
        "You can pick your own password later with `/setpassword`.",
        "",
        "Set your realmlist before you log in:",
        "",
        "```",
        "set realmlist logon.arenacraft.fun",
        "```",
        "> Located in Data/enUS/realmlist.wtf",
      ].join("\n"),
    );
  });

  test("password reset omits the realmlist hint", () => {
    const reply = formatRegisterReply("alice", "hunter2", "Password reset! Your new login:", false);

    expect(reply).toBe(
      [
        "Password reset! Your new login:",
        "",
        "**Username**  => `alice`",
        "**Password**  => `hunter2`",
        "",
        "You can pick your own password later with `/setpassword`.",
      ].join("\n"),
    );
    expect(reply).not.toContain("realmlist");
  });
});
