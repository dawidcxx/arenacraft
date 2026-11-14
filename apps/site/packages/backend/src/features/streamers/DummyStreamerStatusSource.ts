import type { Streamer } from "@arenacraft/common";
import type { StreamerStatusSource } from "./StreamerStatusSource";

const MINUTE_MS = 60 * 1000;
const HOUR_MS = 60 * MINUTE_MS;

/**
 * Local development fixtures, used with `DATA_SOURCE=dummy` (the default). They deliberately cover
 * every state the website renders: two featured channels (one on top, one behind a bigger channel),
 * a small channel, and affiliated channels that are offline.
 *
 * The list is independent of `STREAMER_WHITELIST` so the UI can be exercised without adding fake
 * channels to the production whitelist.
 */
export class DummyStreamerStatusSource implements StreamerStatusSource {
  constructor(private readonly now: () => number = Date.now) {}

  async statuses(): Promise<Streamer[]> {
    const now = this.now();

    return [
      {
        login: "healbotter",
        name: "Healbotter",
        avatarUrl: null,
        live: {
          title: "arenacraft soloq — pushing 2.2k on disc priest",
          game: "World of Warcraft",
          viewers: 143,
          startedAt: now - 3 * HOUR_MS,
          thumbnailUrl: null,
        },
      },
      {
        login: "frostmagee",
        name: "Frostmagee",
        avatarUrl: null,
        live: {
          title: "SoloQ queue, no addons challenge",
          game: "World of Warcraft",
          viewers: 412,
          startedAt: now - 47 * MINUTE_MS,
          thumbnailUrl: null,
        },
      },
      {
        login: "oldgladiator",
        name: "Oldgladiator",
        avatarUrl: null,
        live: null,
      },
      {
        login: "arenacraft",
        name: "Arenacraft",
        avatarUrl: null,
        live: {
          title: "ARENACRAFT 3v3 cup — round two",
          game: "World of Warcraft",
          viewers: 1204,
          startedAt: now - 94 * MINUTE_MS,
          thumbnailUrl: null,
        },
      },
      {
        login: "boomkin",
        name: "Boomkin",
        avatarUrl: null,
        live: null,
      },
      {
        login: "clapmaster",
        name: "Clapmaster",
        avatarUrl: null,
        live: {
          title: "Chill arena with viewers",
          game: "World of Warcraft",
          viewers: 27,
          startedAt: now - 12 * MINUTE_MS,
          thumbnailUrl: null,
        },
      },
    ];
  }
}
