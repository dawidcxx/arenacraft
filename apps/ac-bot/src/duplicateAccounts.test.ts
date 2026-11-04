import { describe, expect, test } from "bun:test";

import { buildAccountFlags, looseAccountKey } from "./duplicateAccounts";

const accounts = [
  { id: 1, username: "Venruki", discordId: null },
  { id: 2, username: "venruki", discordId: null },
  { id: 3, username: "Kalvish", discordId: null },
  { id: 4, username: "athene", discordId: null },
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
  test("flags lookalike names", () => {
    const flags = buildAccountFlags(accounts);

    expect(flags.map((flag) => flag.kind)).toEqual(["lookalike-name"]);
    expect(flags[0]!.accounts.map((account) => account.id)).toEqual([1, 2]);
  });

  test("flags one Discord ID that owns multiple accounts", () => {
    const flags = buildAccountFlags([
      { id: 1, username: "Venruki", discordId: "100" },
      { id: 3, username: "Kalvish", discordId: "100" },
    ]);

    const duplicate = flags.find((flag) => flag.kind === "duplicate-owner");
    expect(duplicate?.accounts.map((account) => account.id)).toEqual([3, 1]);
  });

  test("flags accounts with no owner that match no current member", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "Venruki", discordId: "100" },
        { id: 2, username: "Kalvish", discordId: null },
        { id: 4, username: "athene", discordId: null },
      ],
      [
        { id: "100", username: "Venruki" },
        { id: "101", username: "kalvish" },
      ],
    );

    const unconnected = flags.find((flag) => flag.kind === "unconnected");
    expect(unconnected?.accounts.map((account) => account.id)).toEqual([4]);
  });

  test("flags owned accounts whose handle is now held by someone else", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "venruki", discordId: "100" },
        { id: 2, username: "athene", discordId: null },
      ],
      [
        { id: "100", username: "venruki2" },
        { id: "200", username: "venruki" },
      ],
    );

    const reclaimed = flags.find((flag) => flag.kind === "handle-reclaimed");
    expect(reclaimed?.accounts.map((account) => account.id)).toEqual([1]);
  });

  test("flags unowned accounts that match a member who already owns another account", () => {
    const flags = buildAccountFlags(
      [
        { id: 1, username: "bob", discordId: "100" },
        { id: 2, username: "alice", discordId: null },
      ],
      [{ id: "100", username: "alice" }],
    );

    const blocked = flags.find((flag) => flag.kind === "member-already-owns-account");
    expect(blocked?.accounts.map((account) => account.id)).toEqual([2]);
  });

  test("returns no flags for distinct accounts", () => {
    const flags = buildAccountFlags([
      { id: 1, username: "a", discordId: "100" },
      { id: 2, username: "b", discordId: "200" },
    ]);

    expect(flags).toEqual([]);
  });
});
