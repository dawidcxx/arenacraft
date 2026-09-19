import { z } from "zod";

// Mirrors the JSON published by the core (see src/game/Arenacraft/soloq/SoloqEvents.cpp).
export const MATCHUP_CHANNEL = "soloq-matchup";
export const MATCHUP_ENDED_CHANNEL = "soloq-matchup-ended";

export const ParticipantSchema = z.object({
  guid: z.string(),
  characterName: z.string(),
  accountId: z.number(),
  accountName: z.string(),
  classId: z.number(),
  specIndex: z.number(),
  role: z.string(),
  teamId: z.number(),
  faction: z.string(),
  rating: z.number(),
  mmr: z.number(),
});

const TeamSchema = z.object({
  teamId: z.number(),
  faction: z.string(),
  averageRating: z.number(),
  averageMmr: z.number(),
  players: z.array(ParticipantSchema),
});

export const MatchupEventSchema = z.object({
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

export const MatchupEndedEventSchema = z.object({
  event: z.literal("soloq.matchup.ended"),
  instanceId: z.number(),
  finished: z.boolean(),
  players: z.array(ParticipantSchema),
});

export type Participant = z.infer<typeof ParticipantSchema>;
export type MatchupEvent = z.infer<typeof MatchupEventSchema>;
export type MatchupEndedEvent = z.infer<typeof MatchupEndedEventSchema>;
