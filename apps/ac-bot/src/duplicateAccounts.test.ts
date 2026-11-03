import { describe, expect, test } from "bun:test";

import { buildAccountFlags, looseAccountKey } from "./duplicateAccounts";

const accounts = [
  { id: 1, username: "Venruki", lastIp: "10.0.0.1" },
  { id: 2, username: "venruki", lastIp: "10.0.0.2" },
  { id: 3, username: "Kalvish", lastIp: "10.0.0.1" },
  { id: 4, username: "athene", lastIp: "127.0.0.1" },
];

describe("looseAccountKey", () => {
  test("folds case, accents and separators", () => {
    expect(looseAccountKey("John.Doe")).toBe("johndoe");
    expect(looseAccountKey("john_doe")).toBe("johndoe");
    expect(looseAccountKey("Jöhn-Döe")).toBe("johndoe");
  });

  test("returns an empty key when nothing usable remains", () => {
    expect(looseAccountKey("___")).toBe("");
  });
});

describe("buildAccountFlags", () => {
  test("flags lookalike names and shared IPs", () => {
    const flags = buildAccountFlags(accounts, []);

    expect(flags.map((flag) => flag.kind)).toEqual(["lookalike-name", "shared-ip"]);
    expect(flags[0]!.accounts.map((account) => account.id)).toEqual([1, 2]);
    expect(flags[1]!.accounts.map((account) => account.id)).toEqual([3, 1]);
  });

  test("flags one Discord ID linked to multiple accounts", () => {
    const flags = buildAccountFlags(accounts, [
      { accountId: 1, discordId: "100" },
      { accountId: 3, discordId: "100" },
    ]);

    const duplicate = flags.find((flag) => flag.kind === "duplicate-link");
    expect(duplicate?.accounts.map((account) => account.id)).toEqual([3, 1]);
  });

  test("flags unlinked accounts that match no current member", () => {
    const flags = buildAccountFlags(accounts, [{ accountId: 1, discordId: "100" }], [
      { id: "100", username: "Venruki" },
      { id: "101", username: "kalvish" },
    ]);

    const orphaned = flags.find((flag) => flag.kind === "orphaned");
    expect(orphaned?.accounts.map((account) => account.id)).toEqual([4]);
  });

  test("flags linked accounts whose handle is now held by someone else", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "venruki", lastIp: "10.0.0.1" },
        { id: 2, username: "athene", lastIp: "10.0.0.2" },
      ],
      [{ accountId: 1, discordId: "100" }],
      [
        { id: "100", username: "venruki2" },
        { id: "200", username: "venruki" },
      ],
    );

    const reclaimed = flags.find((flag) => flag.kind === "handle-reclaimed");
    expect(reclaimed?.accounts.map((account) => account.id)).toEqual([1]);
  });

  test("flags unlinked accounts that match a member who already owns another account", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "bob", lastIp: "10.0.0.1" },
        { id: 2, username: "alice", lastIp: "10.0.0.2" },
      ],
      [{ accountId: 1, discordId: "100" }],
      [{ id: "100", username: "alice" }],
    );

    const blocked = flags.find((flag) => flag.kind === "member-already-linked");
    expect(blocked?.accounts.map((account) => account.id)).toEqual([2]);
  });

  test("ignores loopback IPs and single-account groups", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "a", lastIp: "127.0.0.1" },
        { id: 2, username: "b", lastIp: "127.0.0.1" },
        { id: 3, username: "c", lastIp: "10.0.0.9" },
      ],
      [],
    );

    expect(flags).toEqual([]);
  });
});
