import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HttpStatusCodes, invariant } from "@arenacraft/common";
import { serve, type WebSocketServerLike } from "@hono/node-server";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
import { cors } from "hono/cors";
import { secureHeaders } from "hono/secure-headers";
import { WebSocketServer } from "ws";
import { LadderApi } from "@/features/ladder/LadderApi";
import { LadderTopSocket } from "@/features/liveLadder/LadderTopSocket";
import { PlayerOverlayApi } from "@/features/playerOverlay/PlayerOverlayApi";
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
  private webSocketServer: WebSocketServer | null = null;

  constructor(
    private readonly config: Config = inject(CONFIG),
    private readonly ladderApi: LadderApi = inject(LadderApi),
    private readonly ladderTopSocket: LadderTopSocket = inject(LadderTopSocket),
    private readonly playerOverlayApi: PlayerOverlayApi = inject(PlayerOverlayApi),
  ) {}

  async init() {
    invariant(this.serverRef === null, "Must not call .init() twice");
    const app = this.buildRoutes();

    // `upgradeWebSocket` hands the upgraded socket to this ws server.
    const webSocketServer = new WebSocketServer({ noServer: true });
    this.webSocketServer = webSocketServer;

    this.serverRef = serve({
      fetch: app.fetch,
      port: this.config.port,
      hostname: this.config.host,
      websocket: { server: webSocketServer as unknown as WebSocketServerLike },
    });

    logger.info(`Server running at http://${this.config.host}:${this.config.port}`, {
      port: this.config.port,
      dataSource: this.config.dataSource,
    });
  }

  async deinit() {
    logger.info("Closing HTTP service");
    const server = this.serverRef;
    if (server === null) return;
    this.serverRef = null;

    await new Promise<void>((resolve) => {
      server.close(() => resolve());
      // `close()` waits for open keep-alive and upgraded connections. An OBS widget would otherwise
      // keep the process alive on SIGTERM, so drop them as soon as shutdown starts.
      (server as { closeAllConnections?: () => void }).closeAllConnections?.();
      this.webSocketServer?.clients.forEach((client) => client.terminate());
      this.webSocketServer = null;
    });
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
    api.route("/live-ladder", this.ladderTopSocket.build());
    api.route("/", this.playerOverlayApi.build());
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