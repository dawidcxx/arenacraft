import { HttpStatusCodes, type Streamer, type StreamerLive } from "@arenacraft/common";
import { Logger } from "@/lib/Logger";
import {
  chunk,
  thumbnailAt,
  TWITCH_MAX_FILTERS,
  TWITCH_REQUEST_TIMEOUT_MS,
} from "./streamers";
import type { StreamerStatusSource } from "./StreamerStatusSource";
import { STREAMER_WHITELIST, type WhitelistedStreamer } from "./whitelist";

const TWITCH_TOKEN_URL = "https://id.twitch.tv/oauth2/token";
const TWITCH_HELIX_URL = "https://api.twitch.tv/helix";

/** Renew slightly before Twitch's `expires_in`, so an in-flight call never races the expiry. */
const TOKEN_REFRESH_MARGIN_MS = 60 * 1000;

/** Card-sized preview; Twitch serves any size from the same template. */
const THUMBNAIL_WIDTH = 440;
const THUMBNAIL_HEIGHT = 248;

export type TwitchCredentials = {
  clientId: string;
  clientSecret: string;
};

/** Narrow fetch signature, so tests can inject a plain function. */
export type TwitchFetch = (input: string, init?: RequestInit) => Promise<Response>;

type HelixUser = {
  login: string;
  display_name: string;
  profile_image_url: string;
};

type HelixStream = {
  user_login: string;
  title: string;
  game_name: string;
  viewer_count: number;
  started_at: string;
  thumbnail_url: string;
};

/**
 * Production source: Twitch Helix for the whitelisted channels, authorised with an app access token
 * from the client-credentials grant. `GET /streams` answers which channels are live and with what
 * title, `GET /users` supplies display names and avatars for the offline ones too.
 *
 * Only the backend talks to Twitch; the client secret never leaves the server. Without credentials
 * the whitelist still renders, but nobody is reported live: the site stays useful and no channel is
 * ever shown as live by accident.
 */
export class TwitchStreamerStatusSource implements StreamerStatusSource {
  private token: { value: string; expiresAt: number } | null = null;
  /** Concurrent Helix calls share one token request instead of each minting their own. */
  private tokenRequest: Promise<string> | null = null;
  private warnedAboutMissingCredentials = false;

  constructor(
    private readonly credentials: TwitchCredentials | null,
    private readonly whitelist: readonly WhitelistedStreamer[] = STREAMER_WHITELIST,
    private readonly now: () => number = Date.now,
    private readonly fetchFn: TwitchFetch = (input, init) => globalThis.fetch(input, init),
  ) {}

  async statuses(): Promise<Streamer[]> {
    if (this.whitelist.length === 0) return [];

    if (this.credentials === null) {
      if (!this.warnedAboutMissingCredentials) {
        this.warnedAboutMissingCredentials = true;
        logger.warn(
          "TWITCH_CLIENT_ID/TWITCH_CLIENT_SECRET are not set; the streamer zone reports every channel offline",
        );
      }
      return this.offlineStatuses();
    }

    const credentials = this.credentials;
    const [users, streams] = await Promise.all([
      this.helixFilters<HelixUser>("/users", credentials, "login"),
      this.helixFilters<HelixStream>("/streams", credentials, "user_login"),
    ]);

    const userByLogin = new Map(users.map((user) => [user.login.toLowerCase(), user]));
    const streamByLogin = new Map(streams.map((stream) => [stream.user_login.toLowerCase(), stream]));
    const now = this.now();

    return this.whitelist.map((channel) => {
      const login = channel.login.toLowerCase();
      const user = userByLogin.get(login);
      const stream = streamByLogin.get(login);

      return {
        login,
        name: user?.display_name ?? channel.name,
        avatarUrl: user?.profile_image_url ?? null,
        live: stream === undefined ? null : toLive(stream, now),
      };
    });
  }

  /** Queries one Helix endpoint for every whitelisted login, in filter-sized batches. */
  private async helixFilters<T>(
    path: string,
    credentials: TwitchCredentials,
    filter: string,
  ): Promise<T[]> {
    const logins = this.whitelist.map((channel) => channel.login.toLowerCase());

    const pages = await Promise.all(
      chunk(logins, TWITCH_MAX_FILTERS).map(async (group) => {
        const params = new URLSearchParams({ first: String(TWITCH_MAX_FILTERS) });
        for (const login of group) params.append(filter, login);
        return this.helixGet<T>(path, credentials, params);
      }),
    );

    return pages.flat();
  }

  private async helixGet<T>(
    path: string,
    credentials: TwitchCredentials,
    params: URLSearchParams,
    retried = false,
  ): Promise<T[]> {
    const token = await this.accessToken(credentials);
    const response = await this.fetchFn(`${TWITCH_HELIX_URL}${path}?${params.toString()}`, {
      headers: {
        "Client-Id": credentials.clientId,
        Authorization: `Bearer ${token}`,
      },
      signal: AbortSignal.timeout(TWITCH_REQUEST_TIMEOUT_MS),
    });

    // Tokens can be revoked before they expire; one silent retry covers that.
    if (response.status === HttpStatusCodes.UNAUTHORIZED && !retried) {
      if (this.token?.value === token) this.token = null;
      return this.helixGet<T>(path, credentials, params, true);
    }

    if (!response.ok) {
      throw new Error(`Twitch ${path} failed with ${response.status}`);
    }

    const body = (await response.json()) as { data?: T[] };
    return body.data ?? [];
  }

  private async accessToken(credentials: TwitchCredentials): Promise<string> {
    const now = this.now();
    if (this.token !== null && now < this.token.expiresAt) return this.token.value;

    this.tokenRequest ??= this.requestToken(credentials, now).finally(() => {
      this.tokenRequest = null;
    });

    return this.tokenRequest;
  }

  private async requestToken(credentials: TwitchCredentials, now: number): Promise<string> {
    const response = await this.fetchFn(TWITCH_TOKEN_URL, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({
        client_id: credentials.clientId,
        client_secret: credentials.clientSecret,
        grant_type: "client_credentials",
      }).toString(),
      signal: AbortSignal.timeout(TWITCH_REQUEST_TIMEOUT_MS),
    });

    if (!response.ok) {
      throw new Error(`Twitch token request failed with ${response.status}`);
    }

    const body = (await response.json()) as { access_token?: unknown; expires_in?: unknown };
    if (typeof body.access_token !== "string") {
      throw new Error("Twitch token response did not contain an access_token");
    }

    const lifetimeMs = typeof body.expires_in === "number" ? body.expires_in * 1000 : 60 * 60 * 1000;
    this.token = {
      value: body.access_token,
      expiresAt: now + lifetimeMs - TOKEN_REFRESH_MARGIN_MS,
    };

    return this.token.value;
  }

  private offlineStatuses(): Streamer[] {
    return this.whitelist.map((channel) => ({
      login: channel.login.toLowerCase(),
      name: channel.name,
      avatarUrl: null,
      live: null,
    }));
  }
}

function toLive(stream: HelixStream, now: number): StreamerLive {
  return {
    title: stream.title,
    game: stream.game_name,
    viewers: stream.viewer_count,
    startedAt: startedAtMs(stream.started_at, now),
    thumbnailUrl:
      stream.thumbnail_url === ""
        ? null
        : thumbnailAt(stream.thumbnail_url, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT),
  };
}

/** Twitch sends RFC3339 timestamps; a malformed one falls back to "live now". */
export function startedAtMs(value: string, fallback: number): number {
  const parsed = Date.parse(value);
  return Number.isNaN(parsed) ? fallback : parsed;
}

const logger = Logger.for(TwitchStreamerStatusSource);
