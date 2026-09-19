import { describe, expect, test } from "bun:test";

import { MATCHUP_ENDED_CHANNEL, MATCHUP_CHANNEL, MatchupEndedEventSchema, MatchupEventSchema } from "./types";

const participant = {
  guid: "0x0000000000001000",
  characterName: "Alpha",
  accountId: 1,
  accountName: "ALPHA",
  classId: 1,
  specIndex: 0,
  role: "melee",
  teamId: 0,
  faction: "alliance",
  rating: 1400,
  mmr: 1500,
};

const team = (teamId: number, faction: string) => ({
  teamId,
  faction,
  averageRating: 1400,
  averageMmr: 1500,
  players: [participant],
});

describe("event schemas", () => {
  test("channels match the core contract", () => {
    expect(MATCHUP_CHANNEL).toBe("soloq-matchup");
    expect(MATCHUP_ENDED_CHANNEL).toBe("soloq-matchup-ended");
  });

  test("accepts a well-formed matchup event", () => {
    const parsed = MatchupEventSchema.safeParse({
      event: "soloq.matchup",
      instanceId: 1,
      arenaType: 3,
      matchup: { instanceId: 1, arenaType: 3, mapId: 617, bracketId: 2, queueType: 5, startedAtMs: 1 },
      players: [participant],
      teamA: team(0, "alliance"),
      teamB: team(1, "horde"),
      rating: {
        teamA: { averageRating: 1400, averageMmr: 1500, winDelta: 8, lossDelta: -8 },
        teamB: { averageRating: 1400, averageMmr: 1500, winDelta: 8, lossDelta: -8 },
      },
    });
    expect(parsed.success).toBe(true);
  });

  test("rejects a malformed ended event", () => {
    expect(MatchupEndedEventSchema.safeParse({ event: "soloq.matchup.ended", instanceId: "x" }).success).toBe(false);
  });

  test("accepts a well-formed ended event", () => {
    const parsed = MatchupEndedEventSchema.safeParse({
      event: "soloq.matchup.ended",
      instanceId: 1,
      finished: true,
      players: [participant],
    });
    expect(parsed.success).toBe(true);
  });
});
