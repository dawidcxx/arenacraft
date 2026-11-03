import { describe, expect, test } from "bun:test";

import { formatLegacyConflict, planLegacyLinks } from "./legacyLink";

describe("planLegacyLinks", () => {
  test("links unlinked accounts to the matching member case-insensitively", () => {
    const plan = planLegacyLinks(
      [
        { id: 1, username: "venruki" },
        { id: 2, username: "kalvish" },
      ],
      [],
      [
        { id: "100", username: "Venruki" },
        { id: "200", username: "KALVISH" },
      ],
    );

    expect(plan.links).toEqual([
      { accountId: 1, username: "venruki", discordId: "100" },
      { accountId: 2, username: "kalvish", discordId: "200" },
    ]);
    expect(plan.conflicts).toEqual([]);
  });

  test("leaves accounts with no matching member for manual resolution", () => {
    const plan = planLegacyLinks([{ id: 1, username: "athene" }], [], [{ id: "100", username: "venruki" }]);

    expect(plan.links).toEqual([]);
    expect(plan.conflicts.map((conflict) => conflict.kind)).toEqual(["no-member"]);
  });

  test("does not link a member who already owns another account", () => {
    const plan = planLegacyLinks(
      [{ id: 1, username: "venruki" }],
      [{ accountId: 9, discordId: "100" }],
      [{ id: "100", username: "Venruki" }],
    );

    expect(plan.links).toEqual([]);
    expect(plan.conflicts.map((conflict) => conflict.kind)).toEqual(["member-already-linked"]);
  });

  test("flags a linked account whose old handle is now held by someone else", () => {
    const plan = planLegacyLinks(
      [{ id: 1, username: "venruki" }],
      [{ accountId: 1, discordId: "100" }],
      [
        { id: "100", username: "venruki2" },
        { id: "200", username: "venruki" },
      ],
    );

    expect(plan.links).toEqual([]);
    expect(plan.conflicts).toEqual([
      { kind: "handle-reclaimed", account: { id: 1, username: "venruki" }, discordId: "100", memberId: "200" },
    ]);
  });

  test("does not flag a linked account when the owner still holds the handle", () => {
    const plan = planLegacyLinks(
      [{ id: 1, username: "venruki" }],
      [{ accountId: 1, discordId: "100" }],
      [{ id: "100", username: "venruki" }],
    );

    expect(plan.conflicts).toEqual([]);
  });
});

describe("formatLegacyConflict", () => {
  test("mentions both sides of a reclaimed handle", () => {
    expect(
      formatLegacyConflict({
        kind: "handle-reclaimed",
        account: { id: 1, username: "venruki" },
        discordId: "100",
        memberId: "200",
      }),
    ).toContain("<@100>");
  });
});
