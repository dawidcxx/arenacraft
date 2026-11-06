import { Hono } from "hono";

export function healthRoutes(): Hono {
  const routes = new Hono();

  // Liveness probe: unauthenticated so container/orchestrator health checks need no token.
  routes.on(["GET", "HEAD"], "/", (c) => c.json({ ok: true }));

  return routes;
}
