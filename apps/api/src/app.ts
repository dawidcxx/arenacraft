import { Hono } from "hono";
import { cors } from "hono/cors";
import { HTTPException } from "hono/http-exception";

import type { LadderEntry } from "./ladder/ladder";
import { logger } from "./logger";
import { basicTokenAuth } from "./middleware/auth";
import { healthRoutes } from "./routes/health";
import { ladderRoutes } from "./routes/ladder";

export interface AppDeps {
  ladder: () => Promise<LadderEntry[]>;
  corsOrigin: string;
  cacheTtlSeconds: number;
  authTokens: string[];
}

const ALLOWED_METHODS = ["GET", "HEAD", "OPTIONS"] as const;

export function createApp(deps: AppDeps): Hono {
  const app = new Hono();

  // Applied to every response, including errors.
  app.use("*", async (c, next) => {
    c.header("X-Content-Type-Options", "nosniff");
    c.header("Referrer-Policy", "no-referrer");
    c.header("Cross-Origin-Resource-Policy", "cross-origin");
    await next();
  });

  // CORS preflights are answered here, before the auth and method checks. Query strings are ignored
  // by the routes on purpose: they cannot be used to bypass the snapshot cache.
  app.use(
    "*",
    cors({
      origin: deps.corsOrigin,
      allowMethods: [...ALLOWED_METHODS],
      allowHeaders: ["Authorization"],
      maxAge: 86400,
    }),
  );

  app.use("*", async (c, next) => {
    if (c.req.method !== "GET" && c.req.method !== "HEAD") {
      c.header("Allow", ALLOWED_METHODS.join(", "));
      return c.json({ error: "method not allowed" }, 405);
    }
    await next();
  });

  app.route("/health", healthRoutes());
  app.route(
    "/ladder",
    ladderRoutes({
      ladder: deps.ladder,
      cacheTtlSeconds: deps.cacheTtlSeconds,
      requireToken: basicTokenAuth(deps.authTokens),
    }),
  );

  app.notFound((c) => c.json({ error: "not found" }, 404));

  app.onError((error, c) => {
    if (error instanceof HTTPException) return error.getResponse();
    logger.error("unhandled request error", error);
    return c.json({ error: "internal error" }, 500);
  });

  return app;
}
