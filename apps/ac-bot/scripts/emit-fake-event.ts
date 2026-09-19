/**
 * Publishes fake soloq events to Redis so the bot's voice-room flow can be
 * exercised without running the WoW core.
 *
 * Usage:
 *   bun run scripts/emit-fake-event.ts --channel matchup --instance 123 \
 *       --team-a ALPHA,BRAVO,CHARLIE --team-b DELTA,ECHO,FOXTROT
 *   bun run scripts/emit-fake-event.ts --channel ended --instance 123
 *   bun run scripts/emit-fake-event.ts --channel both --delay 5000
 *
 * Account names must match Discord usernames (the bot maps them case-
 * insensitively). Anything omitted falls back to a default set.
 */
import { Redis } from "ioredis";

import {
  MATCHUP_CHANNEL,
  MATCHUP_ENDED_CHANNEL,
  MatchupEndedEventSchema,
  MatchupEventSchema,
} from "../src/types";
import type { Participant } from "../src/types";

const ROLE_SLOTS = ["melee", "caster", "healer"] as const;
const CLASSES = [1, 3, 5, 8, 9, 11];

function parseArgs(argv: string[]): Record<string, string> {
  const result: Record<string, string> = {};
  for (let i = 0; i < argv.length; i++) {
    const token = argv[i]!;
    if (!token.startsWith("--")) continue;
    const [key, inline] = token.slice(2).split("=", 2);
    if (key === undefined) continue;
    result[key] = inline ?? argv[++i] ?? "";
  }
  return result;
}

function accountList(value: string | undefined, fallback: string[]): string[] {
  if (!value) return fallback;
  return value
    .split(",")
    .map((name) => name.trim().toUpperCase())
    .filter(Boolean);
}

function makeParticipant(accountName: string, slot: number, teamId: 0 | 1, index: number): Participant {
  return {
    guid: `0x${(0x1000 + index).toString(16).padStart(16, "0")}`,
    characterName: accountName.charAt(0) + accountName.slice(1).toLowerCase(),
    accountId: index + 1,
    accountName,
    classId: CLASSES[index % CLASSES.length]!,
    specIndex: 0,
    role: ROLE_SLOTS[slot % ROLE_SLOTS.length]!,
    teamId,
    faction: teamId === 0 ? "alliance" : "horde",
    rating: 1400 + index * 10,
    mmr: 1500 + index * 10,
  };
}

function average(values: number[]): number {
  return Math.round(values.reduce((sum, value) => sum + value, 0) / Math.max(1, values.length));
}

async function main(): Promise<void> {
  const args = parseArgs(process.argv.slice(2));
  if (args.help !== undefined) {
    console.log("See the header of scripts/emit-fake-event.ts for usage.");
    return;
  }

  const channel = args.channel ?? "both";
  const instanceId = Number(args.instance ?? 9000);
  const delayMs = Number(args.delay ?? 5000);
  const finished = args.finished !== "false";

  const teamANames = accountList(args["team-a"], ["ALPHA", "BRAVO", "CHARLIE"]);
  const teamBNames = accountList(args["team-b"], ["DELTA", "ECHO", "FOXTROT"]);

  const teamAPlayers = teamANames.map((name, index) => makeParticipant(name, index, 0, index));
  const teamBPlayers = teamBNames.map((name, index) =>
    makeParticipant(name, index, 1, teamANames.length + index),
  );
  const players = [...teamAPlayers, ...teamBPlayers];

  const avgMmrA = average(teamAPlayers.map((player) => player.mmr));
  const avgMmrB = average(teamBPlayers.map((player) => player.mmr));

  const matchup = MatchupEventSchema.parse({
    event: "soloq.matchup",
    instanceId,
    arenaType: 3,
    matchup: { instanceId, arenaType: 3, mapId: 617, bracketId: 2, queueType: 5, startedAtMs: Date.now() },
    players,
    teamA: {
      teamId: 0,
      faction: "alliance",
      averageRating: average(teamAPlayers.map((player) => player.rating)),
      averageMmr: avgMmrA,
      players: teamAPlayers,
    },
    teamB: {
      teamId: 1,
      faction: "horde",
      averageRating: average(teamBPlayers.map((player) => player.rating)),
      averageMmr: avgMmrB,
      players: teamBPlayers,
    },
    rating: {
      teamA: { averageRating: average(teamAPlayers.map((p) => p.rating)), averageMmr: avgMmrA, winDelta: 8, lossDelta: -8 },
      teamB: { averageRating: average(teamBPlayers.map((p) => p.rating)), averageMmr: avgMmrB, winDelta: 8, lossDelta: -8 },
    },
  });

  const ended = MatchupEndedEventSchema.parse({
    event: "soloq.matchup.ended",
    instanceId,
    finished,
    players,
  });

  const redis = new Redis(process.env.REDIS_URL ?? "redis://127.0.0.1:6379");
  try {
    if (channel === "matchup" || channel === "both") {
      await redis.publish(MATCHUP_CHANNEL, JSON.stringify(matchup));
      console.log(`published ${MATCHUP_CHANNEL} instance=${instanceId} players=${players.map((p) => p.accountName).join(",")}`);
    }
    if (channel === "both") await new Promise((resolve) => setTimeout(resolve, delayMs));
    if (channel === "ended" || channel === "both") {
      await redis.publish(MATCHUP_ENDED_CHANNEL, JSON.stringify(ended));
      console.log(`published ${MATCHUP_ENDED_CHANNEL} instance=${instanceId} finished=${finished}`);
    }
  } finally {
    await redis.quit();
  }
}

await main();
