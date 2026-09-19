import { describe, expect, test } from "bun:test";

import { groupGameRooms } from "./voiceRooms";

const PATTERN = /^game-(\d+)-(\d+)$/;

describe("groupGameRooms", () => {
  test("groups pairs by room number and orders channels by suffix", () => {
    const rooms = groupGameRooms(
      [
        { id: "b", name: "game-1-2" },
        { id: "a", name: "game-1-1" },
        { id: "d", name: "game-2-2" },
        { id: "c", name: "game-2-1" },
        { id: "x", name: "Waiting Room" },
      ],
      PATTERN,
    );

    expect(rooms).toEqual([
      { group: 1, first: { id: "a", name: "game-1-1" }, second: { id: "b", name: "game-1-2" } },
      { group: 2, first: { id: "c", name: "game-2-1" }, second: { id: "d", name: "game-2-2" } },
    ]);
  });

  test("ignores incomplete groups and unrelated channels", () => {
    expect(groupGameRooms([{ id: "a", name: "game-1-1" }], PATTERN)).toEqual([]);
    expect(groupGameRooms([{ id: "a", name: "Lobby" }], PATTERN)).toEqual([]);
  });
});
