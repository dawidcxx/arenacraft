import { array, InferOutput, literal, number, object, string, variant } from "valibot";

export const LiveLadderPlayer = object({
  rank: number(),
  name: string(),
  class: string(),
  rating: number(),
  wins: number(),
  losses: number(),
});

export type LiveLadderPlayer = InferOutput<typeof LiveLadderPlayer>;

// Full state. Sent on connect and every 10 seconds to heal any client drift.
export const LiveLadderSnapshotMessage = object({
  type: literal("snapshot"),
  players: array(LiveLadderPlayer),
  updatedAt: number(),
});

export type LiveLadderSnapshotMessage = InferOutput<typeof LiveLadderSnapshotMessage>;

// Only the rows that changed since the previous poll, keyed by `rank`. `size` lets the
// client drop rows when the top list shrinks.
export const LiveLadderUpdateMessage = object({
  type: literal("update"),
  players: array(LiveLadderPlayer),
  size: number(),
  updatedAt: number(),
});

export type LiveLadderUpdateMessage = InferOutput<typeof LiveLadderUpdateMessage>;

export const LiveLadderMessage = variant("type", [
  LiveLadderSnapshotMessage,
  LiveLadderUpdateMessage,
]);

export type LiveLadderMessage = InferOutput<typeof LiveLadderMessage>;