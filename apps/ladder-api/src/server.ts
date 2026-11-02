import type { LadderEntry } from "./ladder";
import { logger } from "./logger";

export interface HandlerDeps {
  ladder: () => Promise<LadderEntry[]>;
  corsOrigin: string;
  cacheTtlSeconds: number;
}

const ALLOWED_METHODS = "GET, HEAD, OPTIONS";

function baseHeaders(deps: HandlerDeps): Headers {
  return new Headers({
    "Access-Control-Allow-Origin": deps.corsOrigin,
    "Access-Control-Allow-Methods": ALLOWED_METHODS,
    "X-Content-Type-Options": "nosniff",
    "Referrer-Policy": "no-referrer",
    "Cross-Origin-Resource-Policy": "cross-origin",
  });
}

function json(deps: HandlerDeps, status: number, body: unknown, cacheSeconds = 0): Response {
  const headers = baseHeaders(deps);
  headers.set("Content-Type", "application/json; charset=utf-8");
  headers.set("Cache-Control", cacheSeconds > 0 ? `public, max-age=${cacheSeconds}` : "no-store");
  return new Response(JSON.stringify(body), { status, headers });
}

/**
 * Read-only HTTP surface. Query strings are ignored on purpose: every request for a path maps to the
 * same cached snapshot, so varying parameters cannot be used to bypass the cache and reach the database.
 */
export function createHandler(deps: HandlerDeps): (request: Request) => Promise<Response> {
  return async (request) => {
    const { pathname } = new URL(request.url);
    const method = request.method;

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

    if (pathname === "/health") return json(deps, 200, { ok: true });

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
