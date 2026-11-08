import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HttpStatusCodes, invariant } from "@arenacraft/common";
import { serve } from "@hono/node-server";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
import { cors } from "hono/cors";
import { secureHeaders } from "hono/secure-headers";
import { LadderApi } from "@/features/ladder/LadderApi";
import { honoJsErrorHandler } from "@/lib/honoJsApiErrorHandler";
import { Logger } from "@/lib/Logger";
import { requestObservability } from "@/lib/requestObservability";
import { serveStaticAssets } from "@/lib/serveStaticAssets";
import { CONFIG, type Config } from "./Config";
import type { OnDeinit } from "./OnDeinit";
import type { OnInit } from "./OnInit";

const ALLOWED_API_METHODS = ["GET", "HEAD", "OPTIONS"] as const;

@injectable()
export class HttpServer implements OnDeinit, OnInit {
  private serverRef: ReturnType<typeof serve> | null = null;

  constructor(
    private readonly config: Config = inject(CONFIG),
    private readonly ladderApi: LadderApi = inject(LadderApi),
  ) {}

  async init() {
    invariant(this.serverRef === null, "Must not call .init() twice");
    const app = this.buildRoutes();

    this.serverRef = serve({
      fetch: app.fetch,
      port: this.config.port,
      hostname: this.config.host,
    });

    logger.info(`Server running at http://${this.config.host}:${this.config.port}`, {
      port: this.config.port,
      dataSource: this.config.dataSource,
    });
  }

  async deinit() {
    logger.info("Closing HTTP service");
    if (this.serverRef) {
      await new Promise<void>((res) => this.serverRef!.close(() => res()));
      this.serverRef = null;
    }
  }

  private buildRoutes() {
    const app = new Hono();
    const api = new Hono();

    app.use("*", requestObservability());
    applySecurityMiddlewares(app, this.config);

    const frontendDistPath = resolve(dirname(fileURLToPath(import.meta.url)), "public");
    api.onError(honoJsErrorHandler(Logger.for("api.onError")));

    // This service is a read-only ladder proxy. Everything except health checks is behind this.
    api.use("*", async (c, next) => {
      if (c.req.method !== "GET" && c.req.method !== "HEAD" && c.req.method !== "OPTIONS") {
        c.header("Allow", ALLOWED_API_METHODS.join(", "));
        return c.json(
          { code: "MethodNotAllowed", message: "This API is read-only" },
          HttpStatusCodes.METHOD_NOT_ALLOWED,
        );
      }
      await next();
    });

    api.on(["GET", "HEAD"], "/health", (c) => c.json({ ok: true }));
    api.route("/", this.ladderApi.build());
    app.route("/api", api);
    app.get("*", serveStaticAssets(frontendDistPath));

    logger.info("Registered API routes", { routes: app.routes.map((it) => [it.method, it.path]) });

    return app;
  }
}

const logger = Logger.for(HttpServer);

function applySecurityMiddlewares(app: Hono, config: Config) {
  app.use("*", secureHeaders());

  if (config.corsOrigins.length > 0) {
    app.use(
      "*",
      cors({
        origin: config.corsOrigins,
        allowMethods: [...ALLOWED_API_METHODS],
        allowHeaders: ["Content-Type", "X-Request-Id"],
        maxAge: 600,
      }),
    );
  }
}