import { Hono } from "hono";
import type { MiddlewareHandler } from "hono";

import type { LadderEntry } from "../ladder/ladder";
import { logger } from "../logger";

export interface LadderRouteDeps {
  ladder: () => Promise<LadderEntry[]>;
  cacheTtlSeconds: number;
  requireToken: MiddlewareHandler;
}

export function ladderRoutes(deps: LadderRouteDeps): Hono {
  const routes = new Hono();

  routes.on(["GET", "HEAD"], "/soloq", deps.requireToken, async (c) => {
    try {
      const entries = await deps.ladder();
      c.header("Cache-Control", `private, max-age=${deps.cacheTtlSeconds}`);
      return c.json(entries);
    } catch (error) {
      logger.error("ladder query failed", error);
      return c.json({ error: "ladder temporarily unavailable" }, 503);
    }
  });

  return routes;
}
