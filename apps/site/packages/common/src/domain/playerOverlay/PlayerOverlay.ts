import { array, InferOutput, literal, nullable, number, object, string, union } from "valibot";

export const OverlayMatchResult = union([literal("W"), literal("L")]);
export type OverlayMatchResult = InferOutput<typeof OverlayMatchResult>;

export const OverlayRecord = object({
  wins: number(),
  losses: number(),
});

export type OverlayRecord = InferOutput<typeof OverlayRecord>;

// One streamer's SoloQ card for the personal OBS overlay (`/overlay/:token`).
export const PlayerOverlay = object({
  name: string(),
  class: string(),
  // Null until the character has a SoloQ team.
  rating: nullable(number()),
  // Season record, the same numbers the ladder shows.
  all: OverlayRecord,
  // Matches finished since midnight, server time.
  today: OverlayRecord,
  // Most recent SoloQ results, oldest first, at most `PLAYER_OVERLAY_LAST_MATCHES`.
  lastMatches: array(OverlayMatchResult),
  updatedAt: number(),
});

export type PlayerOverlay = InferOutput<typeof PlayerOverlay>;

export const PLAYER_OVERLAY_LAST_MATCHES = 5;