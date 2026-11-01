import { describe, expect, test } from "bun:test";

import { formatDuration, formatEnded, formatMatchup } from "./auditLog";
import type { MatchupEndedEvent, MatchupEvent, Participant } from "./types";

const EMOJI: Record<number, string> = {
  1: "<:warrior:100>",
  5: "<:priest:101>",
  8: "<:mage:102>",
  11: "<:druid:103>",
};

const emoji = (classId: number) => EMOJI[classId];

function player(overrides: Partial<Participant> & Pick<Participant, "characterName" | "classId" | "teamId">): Participant {
  return {
    guid: "0x0000000000000001",
    accountId: 1,
    accountName: "ACC",
    specIndex: 0,
    role: "melee",
    faction: overrides.teamId === 0 ? "alliance" : "horde",
    rating: 1400,
    mmr: 1500,
    ...overrides,
  };
}

const teamA = [
  player({ characterName: "Alpha", classId: 1, teamId: 0, rating: 1450 }),
  player({ characterName: "Beta", classId: 5, teamId: 0, rating: 1480 }),
  player({ characterName: "Gamma", classId: 8, teamId: 0, rating: 1420 }),
];
const teamB = [
  player({ characterName: "Delta", classId: 11, teamId: 1, rating: 1432 }),
  player({ characterName: "Epsilon", classId: 4, teamId: 1, rating: 1410 }),
  player({ characterName: "Zeta", classId: 7, teamId: 1, rating: 1455 }),
];

function matchupEvent(): MatchupEvent {
  const team = (teamId: 0 | 1, players: Participant[]) => ({
    teamId,
    faction: teamId === 0 ? "alliance" : "horde",
    averageRating: 1450,
    averageMmr: 1500,
    players,
  });

  return {
    event: "soloq.matchup",
    instanceId: 1234,
    arenaType: 3,
    matchup: { instanceId: 1234, arenaType: 3, mapId: 572, bracketId: 2, queueType: 5, startedAtMs: 1 },
    players: [...teamA, ...teamB],
    teamA: team(0, teamA),
    teamB: team(1, teamB),
    rating: {
      teamA: { averageRating: 1450, averageMmr: 1500, winDelta: 8, lossDelta: -8 },
      teamB: { averageRating: 1432, averageMmr: 1480, winDelta: 8, lossDelta: -8 },
    },
  };
}

function endedEvent(): MatchupEndedEvent {
  return {
    event: "soloq.matchup.ended",
    instanceId: 1234,
    finished: true,
    winner: "teamA",
    startedAtMs: 1_700_000_000_000,
    endedAtMs: 1_700_000_252_000,
    players: [
      ...teamA.map((participant) => ({ ...participant, ratingDelta: 9, ratingAfter: 1459, mmrAfter: 1509 })),
      ...teamB.map((participant) => ({ ...participant, ratingDelta: -7, ratingAfter: 1425, mmrAfter: 1493 })),
    ],
  };
}

describe("formatMatchup", () => {
  test("renders header, map and both teams with class icons and ratings", () => {
    const text = formatMatchup(matchupEvent(), emoji);

    expect(text.split("\n")).toHaveLength(3);
    expect(text).toContain("⚔️ SoloQ #1234 · 3v3 · Ruins of Lordaeron");
    expect(text).toContain("🟦 <:warrior:100>Alpha 1450 · <:priest:101>Beta 1480 · <:mage:102>Gamma 1420");
    expect(text).toContain("🟥 <:druid:103>Delta 1432 · Epsilon 1410 · Zeta 1455");
  });

  test("omits unknown map and missing class icons", () => {
    const event = matchupEvent();
    event.matchup.mapId = 999;

    const text = formatMatchup(event, () => undefined);
    expect(text).not.toContain("999");
    expect(text).toContain("Alpha 1450");
  });
});

describe("formatEnded", () => {
  test("renders winner, duration and per-player rating deltas", () => {
    const text = formatEnded(endedEvent(), emoji);

    expect(text.split("\n")).toHaveLength(3);
    expect(text).toContain("🏆 SoloQ #1234 · Team A wins · 4m12s");
    expect(text).toContain("🟦 <:warrior:100>Alpha +9 · <:priest:101>Beta +9 · <:mage:102>Gamma +9");
    expect(text).toContain("🟥 <:druid:103>Delta -7 · Epsilon -7 · Zeta -7");
  });

  test("renders an aborted match without rating movement", () => {
    const event = endedEvent();
    event.finished = false;
    event.winner = null;

    const text = formatEnded(event, emoji);
    expect(text).toContain("⚠️ SoloQ #1234 · aborted (invites declined)");
    expect(text).toContain("🟦 Alpha, Beta, Gamma");
    expect(text).toContain("🟥 Delta, Epsilon, Zeta");
    expect(text).not.toContain("+9");
  });
});

describe("formatDuration", () => {
  test("renders minutes and seconds", () => {
    expect(formatDuration(252_000)).toBe("4m12s");
    expect(formatDuration(45_000)).toBe("45s");
    expect(formatDuration(-5)).toBe("0s");
  });
});
