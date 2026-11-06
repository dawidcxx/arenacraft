#!/usr/bin/env bun
/**
 * Publishes fake soloq events to Redis so soloq listeners (the Discord bot,
 * custom tooling, ...) can be exercised without running the WoW core.
 *
 * The payload contract mirrors the JSON published by the core (see
 * src/game/Arenacraft/soloq/SoloqEvents.cpp). This script is deliberately
 * standalone: it does not import from apps/ac-bot.
 *
 * Usage:
 *   bun run scripts/emit_fake_event --channel matchup --instance 123 \
 *       --team-a ALPHA,BRAVO,CHARLIE --team-b DELTA,ECHO,FOXTROT
 *   bun run scripts/emit_fake_event --channel ended --instance 123 --winner teamB
 *   bun run scripts/emit_fake_event --channel both --delay 5000
 *
 * `--finished false` publishes an aborted match; `--winner teamA|teamB` picks
 * the winning side for a finished match (default teamA).
 *
 * Voice moves resolve accountId -> account_discord, so pass real account IDs
 * or use `--team-a-ids` / `--team-b-ids` with Discord user IDs for a purely
 * fake match. Anything omitted falls back to a default set.
 *
 * Set REDIS_URL to target a non-default Redis/Valkey.
 */
import { Redis } from "ioredis";
import { z } from "zod";

const MATCHUP_CHANNEL = "soloq-matchup";
const MATCHUP_ENDED_CHANNEL = "soloq-matchup-ended";

const ParticipantSchema = z.object({
  guid: z.string(),
  characterName: z.string(),
  accountId: z.number(),
  accountName: z.string(),
  discordId: z.string().optional(),
  classId: z.number(),
  specIndex: z.number(),
  role: z.string(),
  teamId: z.number(),
  faction: z.string(),
  rating: z.number(),
  mmr: z.number(),
  side: z.enum(["teamA", "teamB"]).optional(),
  ratingDelta: z.number().int().nullable().optional(),
  ratingAfter: z.number().int().nonnegative().nullable().optional(),
  mmrAfter: z.number().int().nonnegative().nullable().optional(),
});

const TeamSchema = z.object({
  teamId: z.number(),
  faction: z.string(),
  averageRating: z.number(),
  averageMmr: z.number(),
  players: z.array(ParticipantSchema),
});

const MatchupEventSchema = z.object({
  event: z.literal("soloq.matchup"),
  instanceId: z.number(),
  arenaType: z.number(),
  matchup: z.object({
    instanceId: z.number(),
    arenaType: z.number(),
    mapId: z.number(),
    bracketId: z.number(),
    queueType: z.number(),
    startedAtMs: z.number(),
  }),
  players: z.array(ParticipantSchema),
  teamA: TeamSchema,
  teamB: TeamSchema,
  rating: z.object({
    teamA: z.object({ averageRating: z.number(), averageMmr: z.number(), winDelta: z.number(), lossDelta: z.number() }),
    teamB: z.object({ averageRating: z.number(), averageMmr: z.number(), winDelta: z.number(), lossDelta: z.number() }),
  }),
});

const MatchupEndedEventSchema = z.object({
  event: z.literal("soloq.matchup.ended"),
  instanceId: z.number(),
  finished: z.boolean(),
  reason: z.enum(["finished", "cancelled", "declined"]).optional(),
  winner: z.enum(["teamA", "teamB"]).nullable(),
  startedAtMs: z.number().int(),
  endedAtMs: z.number().int(),
  players: z.array(ParticipantSchema),
});

type Participant = z.infer<typeof ParticipantSchema>;

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

function discordIdList(value: string | undefined): string[] {
  if (!value) return [];
  return value
    .split(",")
    .map((id) => id.trim())
    .filter(Boolean);
}

function makeParticipant(
  accountName: string,
  slot: number,
  teamId: 0 | 1,
  index: number,
  discordId?: string,
): Participant {
  return {
    guid: `0x${(0x1000 + index).toString(16).padStart(16, "0")}`,
    characterName: accountName.charAt(0) + accountName.slice(1).toLowerCase(),
    accountId: index + 1,
    accountName,
    discordId,
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
    console.log("See the header of scripts/emit_fake_event for usage.");
    return;
  }

  const channel = args.channel ?? "both";
  const instanceId = Number(args.instance ?? 9000);
  const delayMs = Number(args.delay ?? 5000);
  const finished = args.finished !== "false";
  const winner = args.winner === "teamB" ? "teamB" : "teamA";
  const startedAtMs = Date.now();

  const teamANames = accountList(args["team-a"], ["ALPHA", "BRAVO", "CHARLIE"]);
  const teamBNames = accountList(args["team-b"], ["DELTA", "ECHO", "FOXTROT"]);
  const teamAIds = discordIdList(args["team-a-ids"]);
  const teamBIds = discordIdList(args["team-b-ids"]);

  const teamAPlayers = teamANames.map((name, index) => makeParticipant(name, index, 0, index, teamAIds[index]));
  const teamBPlayers = teamBNames.map((name, index) =>
    makeParticipant(name, index, 1, teamANames.length + index, teamBIds[index]),
  );
  const players = [...teamAPlayers, ...teamBPlayers];

  const avgMmrA = average(teamAPlayers.map((player) => player.mmr));
  const avgMmrB = average(teamBPlayers.map((player) => player.mmr));

  const matchup = MatchupEventSchema.parse({
    event: "soloq.matchup",
    instanceId,
    arenaType: 3,
    matchup: { instanceId, arenaType: 3, mapId: 617, bracketId: 2, queueType: 5, startedAtMs },
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

  const endedPlayers = finished
    ? players.map((player) => {
        const won = (winner === "teamA") === (player.teamId === 0);
        const delta = won ? 8 : -8;
        return {
          ...player,
          ratingDelta: delta,
          ratingAfter: player.rating + delta,
          mmrAfter: player.mmr + delta,
        };
      })
    : players.map((player) => ({ ...player, ratingDelta: null, ratingAfter: null, mmrAfter: null }));

  const ended = MatchupEndedEventSchema.parse({
    event: "soloq.matchup.ended",
    instanceId,
    finished,
    winner: finished ? winner : null,
    startedAtMs,
    endedAtMs: Date.now(),
    players: endedPlayers,
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
