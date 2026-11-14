import { describe, expect, test } from "bun:test";
import { isFeaturedStreamer } from "@arenacraft/common";
import { DummyStreamerStatusSource } from "./DummyStreamerStatusSource";
import { StreamersService } from "./StreamersService";
import { chunk, thumbnailAt } from "./streamers";
import {
  startedAtMs,
  TwitchStreamerStatusSource,
  type TwitchFetch,
} from "./TwitchStreamerStatusSource";

const NOW = Date.UTC(2026, 10, 14, 12, 0, 0);

const CREDENTIALS = { clientId: "client-id", clientSecret: "client-secret" };

const WHITELIST = [
  { login: "arenacraft", name: "Arenacraft" },
  { login: "missingprofile", name: "Missingprofile" },
  { login: "offlineone", name: "Offlineone" },
];

type Stub = {
  fetchFn: TwitchFetch;
  urls: string[];
};

function twitchStub(
  handlers: { streams?: unknown[]; users?: unknown[] } = {},
): Stub {
  const urls: string[] = [];
  const users = handlers.users ?? [
    {
      login: "arenacraft",
      display_name: "Arenacraft Live",
      profile_image_url: "https://cdn.example/avatar.png",
    },
  ];
  const streams = handlers.streams ?? [
    {
      user_login: "arenacraft",
      title: "ARENACRAFT 3v3 cup",
      game_name: "World of Warcraft",
      viewer_count: 900,
      started_at: "2026-11-14T10:00:00Z",
      thumbnail_url: "https://cdn.example/preview-{width}x{height}.jpg",
    },
  ];

  const fetchFn: TwitchFetch = async (input) => {
    urls.push(input);
    if (input.startsWith("https://id.twitch.tv/oauth2/token")) {
      return Response.json({ access_token: "token-1", expires_in: 5_000_000 });
    }
    if (input.includes("/helix/users")) return Response.json({ data: users });
    if (input.includes("/helix/streams")) return Response.json({ data: streams });
    throw new Error(`Unexpected Twitch URL ${input}`);
  };

  return { fetchFn, urls };
}

describe("TwitchStreamerStatusSource", () => {
  test("maps live and offline whitelisted channels, in whitelist order", async () => {
    const stub = twitchStub();
    const source = new TwitchStreamerStatusSource(CREDENTIALS, WHITELIST, () => NOW, stub.fetchFn);

    const streamers = await source.statuses();

    expect(streamers).toEqual([
      {
        login: "arenacraft",
        name: "Arenacraft Live",
        avatarUrl: "https://cdn.example/avatar.png",
        live: {
          title: "ARENACRAFT 3v3 cup",
          game: "World of Warcraft",
          viewers: 900,
          startedAt: Date.UTC(2026, 10, 14, 10, 0, 0),
          thumbnailUrl: "https://cdn.example/preview-440x248.jpg",
        },
      },
      // Twitch returns no profile for this channel, so the whitelist name is the fallback.
      { login: "missingprofile", name: "Missingprofile", avatarUrl: null, live: null },
      { login: "offlineone", name: "Offlineone", avatarUrl: null, live: null },
    ]);
  });

  test("requests the app access token once for repeated snapshots", async () => {
    const stub = twitchStub();
    const source = new TwitchStreamerStatusSource(CREDENTIALS, WHITELIST, () => NOW, stub.fetchFn);

    await source.statuses();
    await source.statuses();

    expect(stub.urls.filter((url) => url.startsWith("https://id.twitch.tv/oauth2/token"))).toHaveLength(1);
  });

  test("renews the token and retries once on 401", async () => {
    const urls: string[] = [];
    let streamAttempts = 0;
    const fetchFn: TwitchFetch = async (input) => {
      urls.push(input);
      if (input.startsWith("https://id.twitch.tv/oauth2/token")) {
        return Response.json({ access_token: `token-${urls.length}`, expires_in: 3600 });
      }
      if (input.includes("/helix/users")) return Response.json({ data: [] });
      if (input.includes("/helix/streams")) {
        streamAttempts += 1;
        return streamAttempts === 1
          ? new Response("unauthorized", { status: 401 })
          : Response.json({ data: [] });
      }
      throw new Error(`Unexpected Twitch URL ${input}`);
    };

    const source = new TwitchStreamerStatusSource(CREDENTIALS, WHITELIST, () => NOW, fetchFn);
    const streamers = await source.statuses();

    expect(streamers.every((streamer) => streamer.live === null)).toBe(true);
    expect(urls.filter((url) => url.startsWith("https://id.twitch.tv/oauth2/token"))).toHaveLength(2);
    expect(streamAttempts).toBe(2);
  });

  test("without credentials the whitelist renders as offline and Twitch is never called", async () => {
    const stub = twitchStub();
    const source = new TwitchStreamerStatusSource(null, WHITELIST, () => NOW, stub.fetchFn);

    const streamers = await source.statuses();

    expect(streamers.map((streamer) => streamer.login)).toEqual([
      "arenacraft",
      "missingprofile",
      "offlineone",
    ]);
    expect(streamers.every((streamer) => streamer.live === null)).toBe(true);
    expect(stub.urls).toHaveLength(0);
  });

  test("an empty whitelist needs no Twitch request", async () => {
    const stub = twitchStub();
    const source = new TwitchStreamerStatusSource(CREDENTIALS, [], () => NOW, stub.fetchFn);

    expect(await source.statuses()).toEqual([]);
    expect(stub.urls).toHaveLength(0);
  });
});

describe("StreamersService", () => {
  test("sorts the source snapshot and caches it", async () => {
    let loads = 0;
    const service = new StreamersService({
      statuses: async () => {
        loads += 1;
        return [
          { login: "offline", name: "Offline", avatarUrl: null, live: null },
          {
            login: "quiet",
            name: "Quiet",
            avatarUrl: null,
            live: {
              title: "SoloQ",
              game: "World of Warcraft",
              viewers: 4,
              startedAt: NOW,
              thumbnailUrl: null,
            },
          },
          {
            login: "featured",
            name: "Featured",
            avatarUrl: null,
            live: {
              title: "arenacraft night",
              game: "World of Warcraft",
              viewers: 1,
              startedAt: NOW,
              thumbnailUrl: null,
            },
          },
        ];
      },
    });

    expect((await service.list()).map((streamer) => streamer.login)).toEqual([
      "featured",
      "quiet",
      "offline",
    ]);
    await service.list();
    expect(loads).toBe(1);
  });
});

describe("DummyStreamerStatusSource", () => {
  test("covers featured, live and offline channels", async () => {
    const streamers = await new DummyStreamerStatusSource(() => NOW).statuses();

    expect(streamers.some(isFeaturedStreamer)).toBe(true);
    expect(streamers.some((streamer) => streamer.live !== null && !isFeaturedStreamer(streamer))).toBe(
      true,
    );
    expect(streamers.some((streamer) => streamer.live === null)).toBe(true);
  });
});

describe("Twitch helpers", () => {
  test("chunk splits oversized batches", () => {
    expect(chunk([], 2)).toEqual([]);
    expect(chunk([1], 2)).toEqual([[1]]);
    expect(chunk([1, 2, 3, 4], 2)).toEqual([
      [1, 2],
      [3, 4],
    ]);
  });

  test("thumbnailAt fills the Helix template", () => {
    expect(thumbnailAt("https://cdn/{width}x{height}.jpg", 440, 248)).toBe(
      "https://cdn/440x248.jpg",
    );
  });

  test("startedAtMs falls back for malformed timestamps", () => {
    expect(startedAtMs("2026-11-14T10:00:00Z", NOW)).toBe(Date.UTC(2026, 10, 14, 10, 0, 0));
    expect(startedAtMs("not a date", NOW)).toBe(NOW);
  });
});
