import { describe, expect, test } from "bun:test";
import { ChannelType } from "discord.js";
import type { Guild } from "discord.js";

import { getConfig } from "./config";
import {
  groupGameRooms,
  matchesChannelName,
  RoomManager,
  selectReturnChannel,
} from "./voiceRooms";

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

describe("matchesChannelName", () => {
  test("matches case-insensitively", () => {
    expect(matchesChannelName("chill room", ["Chill Room"])).toBe(true);
    expect(matchesChannelName("game-1-1", ["Chill Room"])).toBe(false);
  });
});

describe("selectReturnChannel", () => {
  const chill = { id: "chill", name: "Chill Room" };
  const waiting = { id: "waiting", name: "Waiting Room" };
  const returnChannels = new Map([[chill.id, chill]]);

  test("prefers the recorded origin", () => {
    expect(selectReturnChannel("chill", returnChannels, waiting)).toBe(chill);
  });

  test("falls back to the waiting room without a usable origin", () => {
    expect(selectReturnChannel(undefined, returnChannels, waiting)).toBe(waiting);
    // The channel was deleted (or never was a return channel).
    expect(selectReturnChannel("gone", returnChannels, waiting)).toBe(waiting);
  });
});

const BOT_ENV = {
  DISCORD_TOKEN: "token",
  DISCORD_GUILD_ID: "1",
  AUTH_DB_URL: "mysql://acore:acore@127.0.0.1:3306/acore_auth",
  CHARACTER_DB_URL: "mysql://acore:acore@127.0.0.1:3306/acore_characters",
};

/**
 * Minimal guild/voice fake: enough for RoomManager to discover channels and
 * move members around. `setChannel` mirrors what Discord does to the cached
 * voice state, and records every move.
 */
function createGuild(...channelNames: string[]) {
  interface FakeChannel {
    id: string;
    name: string;
    type: ChannelType;
    members: Map<string, FakeMember>;
    permissionOverwrites: { edit: () => Promise<void> };
    denyMoves?: boolean;
  }
  interface FakeMember {
    id: string;
    user: { username: string; bot: boolean };
    voice: {
      channelId: string | null;
      channel: FakeChannel | null;
      setChannel: (target: FakeChannel | null) => Promise<void>;
    };
  }

  const moves: { id: string; channel: string | null }[] = [];
  const channels = new Map<string, FakeChannel>();
  for (const [index, name] of channelNames.entries()) {
    channels.set(`ch-${index}`, {
      id: `ch-${index}`,
      name,
      type: ChannelType.GuildVoice,
      members: new Map(),
      permissionOverwrites: { edit: async () => {} },
    });
  }

  const channel = (name: string): FakeChannel => {
    const found = [...channels.values()].find((candidate) => candidate.name === name);
    if (!found) throw new Error(`no fake channel named ${name}`);
    return found;
  };

  const guild = {
    name: "arenacraft",
    channels: { fetch: async () => channels },
    roles: { everyone: { id: "everyone" } },
    members: { cache: new Map<string, FakeMember>(), fetch: async () => {} },
  };

  const join = (id: string, channelName: string | null): FakeMember => {
    const target = channelName === null ? null : channel(channelName);
    const member: FakeMember = {
      id,
      user: { username: id, bot: false },
      voice: {
        channelId: target?.id ?? null,
        channel: target,
        setChannel: async (destination) => {
          if (destination?.denyMoves) throw new Error("Missing Access");
          member.voice.channel?.members.delete(id);
          member.voice.channel = destination;
          member.voice.channelId = destination?.id ?? null;
          destination?.members.set(id, member);
          moves.push({ id, channel: destination?.id ?? null });
        },
      },
    };
    target?.members.set(id, member);
    guild.members.cache.set(id, member);
    return member;
  };

  const leave = (id: string): void => {
    const member = guild.members.cache.get(id);
    member?.voice.channel?.members.delete(id);
    if (member) {
      member.voice.channel = null;
      member.voice.channelId = null;
    }
  };

  const memberIds = (channelName: string): string[] => [...channel(channelName).members.keys()].sort();

  return { guild: guild as unknown as Guild, channel, join, leave, memberIds, moves };
}

async function managerFor(...channelNames: string[]): Promise<{
  fixture: ReturnType<typeof createGuild>;
  manager: RoomManager;
}> {
  const fixture = createGuild(...channelNames);
  const manager = new RoomManager(fixture.guild, getConfig(BOT_ENV));
  await manager.init();
  return { fixture, manager };
}

const WAITING = "Waiting Room";
const CHILL = "Chill Room";
const SECRET = "Secret Room";
const LOBBY = "Lobby";

describe("RoomManager returns players to their origin", () => {
  test("restores allowed origins and sends everything else to the waiting room", async () => {
    const { fixture, manager } = await managerFor(
      WAITING,
      CHILL,
      LOBBY,
      "game-1-1",
      "game-1-2",
    );
    fixture.join("alice", WAITING);
    fixture.join("bob", CHILL);
    fixture.join("carol", LOBBY);

    await manager.book(7, ["alice", "carol"], ["bob"]);
    expect(fixture.memberIds("game-1-1")).toEqual(["alice", "carol"]);
    expect(fixture.memberIds("game-1-2")).toEqual(["bob"]);

    await manager.release(7);
    expect(fixture.memberIds(WAITING)).toEqual(["alice", "carol"]);
    expect(fixture.memberIds(CHILL)).toEqual(["bob"]);
  });

  test("restores players picked up from the secret room", async () => {
    const { fixture, manager } = await managerFor(WAITING, CHILL, SECRET, "game-1-1", "game-1-2");
    fixture.join("dave", SECRET);

    await manager.book(12, ["dave"], []);
    await manager.release(12);

    expect(fixture.memberIds(SECRET)).toEqual(["dave"]);
    expect(fixture.memberIds(WAITING)).toEqual([]);
  });

  test("never returns a player to a game room picked up there (desync)", async () => {
    const { fixture, manager } = await managerFor(
      WAITING,
      CHILL,
      LOBBY,
      "game-1-1",
      "game-1-2",
      "game-2-1",
      "game-2-2",
    );
    // Stale player left inside a game room by an earlier run.
    fixture.join("alice", "game-1-1");

    await manager.book(8, ["alice"], []);
    expect(fixture.memberIds("game-2-1")).toEqual(["alice"]);

    await manager.release(8);
    expect(fixture.memberIds(WAITING)).toEqual(["alice"]);
    expect(fixture.memberIds("game-1-1")).toEqual([]);
  });

  test("falls back to the waiting room when the origin move is rejected", async () => {
    const { fixture, manager } = await managerFor(WAITING, CHILL, "game-1-1", "game-1-2");
    fixture.join("bob", CHILL);
    fixture.channel(CHILL).denyMoves = true;

    await manager.book(9, [], ["bob"]);
    await manager.release(9);

    expect(fixture.memberIds(CHILL)).toEqual([]);
    expect(fixture.memberIds(WAITING)).toEqual(["bob"]);
  });

  test("does not double-free the same room", async () => {
    const { fixture, manager } = await managerFor(WAITING, CHILL, "game-1-1", "game-1-2");
    fixture.join("bob", CHILL);

    await manager.book(10, [], ["bob"]);
    await Promise.all([manager.release(10), manager.release(10)]);

    expect(fixture.memberIds(CHILL)).toEqual(["bob"]);
    expect(fixture.memberIds(WAITING)).toEqual([]);
  });

  test("drops origins for players who left voice mid-match", async () => {
    const { fixture, manager } = await managerFor(WAITING, CHILL, "game-1-1", "game-1-2");
    fixture.join("alice", CHILL);
    fixture.join("bob", CHILL);

    await manager.book(11, ["alice"], ["bob"]);
    fixture.leave("bob");
    await manager.release(11);

    // Alice still returns to her origin; Bob's entry cannot linger.
    expect(fixture.memberIds(CHILL)).toEqual(["alice"]);
    // Reach into the private room state: the per-room map must be empty.
    const rooms = (manager as unknown as { rooms: { origins: Map<string, string> }[] }).rooms;
    expect(rooms[0]?.origins.size).toBe(0);
  });
});
