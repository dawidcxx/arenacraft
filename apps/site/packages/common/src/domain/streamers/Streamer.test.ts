import { describe, expect, test } from "bun:test";
import { isFeaturedStreamer, sortStreamers, type Streamer } from "./Streamer";

function live(login: string, viewers: number, title: string): Streamer {
  return {
    login,
    name: login,
    avatarUrl: null,
    live: {
      title,
      game: "World of Warcraft",
      viewers,
      startedAt: 1_760_000_000_000,
      thumbnailUrl: null,
    },
  };
}

function offline(login: string): Streamer {
  return { login, name: login, avatarUrl: null, live: null };
}

describe("isFeaturedStreamer", () => {
  test("matches the realm name in any letter case", () => {
    expect(isFeaturedStreamer(live("a", 1, "ARENACRAFT 3v3"))).toBe(true);
    expect(isFeaturedStreamer(live("b", 1, "arenacraft soloq"))).toBe(true);
    expect(isFeaturedStreamer(live("c", 1, "Arenacraft tournament"))).toBe(true);
  });

  test("ignores channels without the keyword and offline channels", () => {
    expect(isFeaturedStreamer(live("a", 1, "Chill arena"))).toBe(false);
    expect(isFeaturedStreamer(offline("a"))).toBe(false);
  });
});

describe("sortStreamers", () => {
  test("puts featured live channels first, then live by viewers, then offline by name", () => {
    const sorted = sortStreamers([
      offline("zulu"),
      live("small", 12, "Chill arena"),
      live("featured-small", 3, "arenacraft queue"),
      offline("alpha"),
      live("big", 900, "SoloQ grind"),
      live("featured-big", 40, "WTS ARENACRAFT boosts"),
    ]);

    expect(sorted.map((streamer) => streamer.login)).toEqual([
      "featured-big",
      "featured-small",
      "big",
      "small",
      "alpha",
      "zulu",
    ]);
  });

  test("does not mutate the input", () => {
    const streamers = [offline("b"), live("a", 1, "Arenacraft")];
    const snapshot = [...streamers];

    sortStreamers(streamers);

    expect(streamers).toEqual(snapshot);
  });
});
