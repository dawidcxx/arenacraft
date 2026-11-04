import { describe, expect, test } from "bun:test";

import { formatAccountFlag } from "./accountAudit";
import type { AccountFlag } from "./duplicateAccounts";

describe("formatAccountFlag", () => {
  const flag: AccountFlag = {
    kind: "unconnected",
    title: "needs review",
    accounts: [{ id: 7, username: "alice", discordId: null, gmLevel: 0 }],
  };

  test("renders the flag and account details", () => {
    const text = formatAccountFlag(flag).join("\n");
    expect(text).toContain("[unconnected] needs review");
    expect(text).toContain("discord=missing");
    expect(text).not.toContain("ip=");
  });
});
