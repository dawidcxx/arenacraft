import { expect, test } from "bun:test";

import { credentialsBlock } from "./registerCommand";

test("the login message carries the username, the password and the configured realmlist", () => {
  const text = credentialsBlock("someone", "s3cret", "logon.arenacraft.example");
  expect(text).toContain("**Username**  => `someone`");
  expect(text).toContain("**Password**  => `s3cret`");
  expect(text).toContain("**Realmlist** => `set realmlist logon.arenacraft.example`");
});

test("without REALMLIST the realmlist line is left out instead of showing a placeholder", () => {
  expect(credentialsBlock("someone", "s3cret", "")).not.toContain("Realmlist");
});
