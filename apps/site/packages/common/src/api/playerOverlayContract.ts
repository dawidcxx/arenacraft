import { InferOutput, object, pipe, regex, string } from "valibot";
import { PlayerOverlay } from "../domain/playerOverlay";

// Tokens are issued by the Discord bot's `/overlay` command (32 url-safe characters).
export const PLAYER_OVERLAY_TOKEN_PATTERN = /^[A-Za-z0-9_-]{32}$/;
export const PlayerOverlayToken = pipe(string(), regex(PLAYER_OVERLAY_TOKEN_PATTERN));

export const PlayerOverlayParams = object({ token: PlayerOverlayToken });
export type PlayerOverlayParams = InferOutput<typeof PlayerOverlayParams>;

export const playerOverlayContract = {
  get: {
    method: "GET",
    path: "/overlay/:token",
    params: PlayerOverlayParams,
    response: PlayerOverlay,
  },
} as const;