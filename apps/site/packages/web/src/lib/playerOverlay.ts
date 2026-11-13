import {
  PLAYER_OVERLAY_LAST_MATCHES,
  type OverlayMatchResult,
  type PlayerOverlay,
} from "@arenacraft/common";

/** `invalid`: the link does not resolve (unknown or revoked token). */
export type PlayerOverlayStatus = "connecting" | "live" | "offline" | "invalid";

export type PlayerOverlayView = {
  status: PlayerOverlayStatus;
  overlay: PlayerOverlay | null;
};

export const INITIAL_PLAYER_OVERLAY: PlayerOverlayView = { status: "connecting", overlay: null };

/** Matches the backend snapshot TTL, so every poll can see fresh data. */
export const PLAYER_OVERLAY_POLL_MS = 5_000;

/** `null` marks an empty slot while the player has fewer than five SoloQ matches. */
export type OverlayMatchSlot = OverlayMatchResult | null;

/** Pads the oldest-first results on the left so the newest match always sits in the last slot. */
export function matchSlots(results: OverlayMatchResult[]): OverlayMatchSlot[] {
  const recent = results.slice(-PLAYER_OVERLAY_LAST_MATCHES);
  const empty: OverlayMatchSlot[] = Array(PLAYER_OVERLAY_LAST_MATCHES - recent.length).fill(null);
  return [...empty, ...recent];
}

/** The fetch client reports failures as `Request failed with <status>: <body>`. */
export function isNotFoundError(error: unknown): boolean {
  return error instanceof Error && error.message.startsWith("Request failed with 404");
}