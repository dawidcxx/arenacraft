import type { Streamer } from "@arenacraft/common";
import { InjectionToken } from "@needle-di/core";

/**
 * Where the streamer zone gets channel status from. Implementations answer for the whole
 * whitelist, so the website can show affiliated channels that are offline as well.
 */
export interface StreamerStatusSource {
  /** One entry per whitelisted channel, live or offline. Order is not significant. */
  statuses(): Promise<Streamer[]>;
}

export const STREAMER_STATUS_SOURCE = new InjectionToken<StreamerStatusSource>(
  "StreamerStatusSource",
);
