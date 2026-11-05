import { Buffer } from "node:buffer";
import { createHash, timingSafeEqual } from "node:crypto";

import type { LadderEntry } from "./ladder";
import { logger } from "./logger";

export interface HandlerDeps {
  ladder: () => Promise<LadderEntry[]>;
  corsOrigin: string;
  cacheTtlSeconds: number;
  authTokens: string[];
}

const ALLOWED_METHODS = "GET, HEAD, OPTIONS";
const BASIC_REALM = 'Basic realm="arenacraft", charset="UTF-8"';

function baseHeaders(deps: HandlerDeps): Headers {
  return new Headers({
    "Access-Control-Allow-Origin": deps.corsOrigin,
    "Access-Control-Allow-Methods": ALLOWED_METHODS,
    "Access-Control-Allow-Headers": "Authorization",
    "X-Content-Type-Options": "nosniff",
    "Referrer-Policy": "no-referrer",
    "Cross-Origin-Resource-Policy": "cross-origin",
  });
}

function json(deps: HandlerDeps, status: number, body: unknown, cacheSeconds = 0): Response {
  const headers = baseHeaders(deps);
  headers.set("Content-Type", "application/json; charset=utf-8");
  headers.set("Cache-Control", cacheSeconds > 0 ? `private, max-age=${cacheSeconds}` : "no-store");
  return new Response(JSON.stringify(body), { status, headers });
}

// Compare digests with a constant-time equality check, so token matching cannot leak the secret
// through response timing.
function tokenMatches(candidate: string, tokens: string[]): boolean {
  const candidateDigest = createHash("sha256").update(candidate).digest();
  let matched = false;
  for (const token of tokens) {
    const tokenDigest = createHash("sha256").update(token).digest();
    if (timingSafeEqual(candidateDigest, tokenDigest)) matched = true;
  }
  return matched;
}

// HTTP Basic auth: any configured token is accepted as either the username or the password, so both
// `user:<token>` and `<token>:` work.
function isAuthorized(request: Request, tokens: string[]): boolean {
  const header = request.headers.get("authorization");
  if (!header) return false;

  const [scheme, credentials] = header.split(" ", 2);
  if (!scheme || scheme.toLowerCase() !== "basic" || !credentials) return false;

  const decoded = Buffer.from(credentials, "base64").toString("utf8");
  const separator = decoded.indexOf(":");
  if (separator < 0) return false;

  const username = decoded.slice(0, separator);
  const password = decoded.slice(separator + 1);
  return tokenMatches(username, tokens) || tokenMatches(password, tokens);
}

function unauthorized(deps: HandlerDeps): Response {
  const response = json(deps, 401, { error: "unauthorized" });
  response.headers.set("WWW-Authenticate", BASIC_REALM);
  return response;
}

/**
 * Read-only HTTP surface. Query strings are ignored on purpose: every request for a path maps to the
 * same cached snapshot, so varying parameters cannot be used to bypass the cache and reach the database.
 */
export function createHandler(deps: HandlerDeps): (request: Request) => Promise<Response> {
  return async (request) => {
    const { pathname } = new URL(request.url);
    const method = request.method;

    // CORS preflights cannot carry credentials, so they are answered before the auth check.
    if (method === "OPTIONS") {
      const headers = baseHeaders(deps);
      headers.set("Access-Control-Max-Age", "86400");
      return new Response(null, { status: 204, headers });
    }

    if (method !== "GET" && method !== "HEAD") {
      const response = json(deps, 405, { error: "method not allowed" });
      response.headers.set("Allow", ALLOWED_METHODS);
      return response;
    }

    // Liveness probe stays open so container health checks do not need a token.
    if (pathname === "/health") return json(deps, 200, { ok: true });

    if (!isAuthorized(request, deps.authTokens)) return unauthorized(deps);

    if (pathname === "/ladder/soloq") {
      try {
        return json(deps, 200, await deps.ladder(), deps.cacheTtlSeconds);
      } catch (error) {
        logger.error("ladder query failed", error);
        return json(deps, 503, { error: "ladder temporarily unavailable" });
      }
    }

    return json(deps, 404, { error: "not found" });
  };
}
