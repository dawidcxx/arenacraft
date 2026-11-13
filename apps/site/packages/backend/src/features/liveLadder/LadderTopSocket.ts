import { HttpStatusCodes } from "@arenacraft/common";
import { upgradeWebSocket, type HttpBindings } from "@hono/node-server";
import { inject, injectable } from "@needle-di/core";
import { Hono, type Context } from "hono";
import { Logger } from "@/lib/Logger";
import { LADDER_TOP_SERVICE, type ILadderTopService } from "./ILadderTopService";
import {
  LIVE_LADDER_MAX_CONNECTIONS,
  WebSocketConnectionLimiter,
} from "./WebSocketConnectionLimiter";

const logger = Logger.for("LadderTopSocket");

@injectable()
export class LadderTopSocket {
  private lastRejectLogAt = 0;
  private suppressedRejections = 0;
  private lastMissingUpgradeLogAt = 0;
  private suppressedMissingUpgrades = 0;

  constructor(
    private readonly service: ILadderTopService = inject(LADDER_TOP_SERVICE),
    private readonly limiter: WebSocketConnectionLimiter = inject(WebSocketConnectionLimiter),
  ) {}

  build() {
    const router = new Hono();
    const unsubscribers = new WeakMap<object, () => void>();

    router.get("/ws", async (c): Promise<Response> => {
      if (c.req.header("upgrade")?.toLowerCase() !== "websocket") {
        this.logMissingUpgrade(c);
        return c.json(
          {
            code: "UpgradeRequired",
            message:
              "This endpoint only speaks WebSocket. Behind a reverse proxy, forward the Upgrade and Connection headers.",
          },
          HttpStatusCodes.UPGRADE_REQUIRED,
        );
      }

      const release = this.limiter.tryAcquire();
      if (release === null) {
        this.logRejection();
        return c.json(
          { code: "TooManyConnections", message: "Live ladder connection limit reached" },
          HttpStatusCodes.SERVICE_UNAVAILABLE,
        );
      }

      // The slot is reserved before the upgrade. If the handshake dies half-way the socket still
      // closes, so release from there as well as from the WebSocket callbacks (release is idempotent).
      (c.env as HttpBindings).incoming.socket.once("close", release);

      const upgrade = upgradeWebSocket(() => ({
        onOpen: (_event, ws) => {
          logger.info("Live ladder WebSocket connected", {
            active: this.limiter.count,
          });
          unsubscribers.set(
            ws,
            this.service.subscribe((event) => {
              if (ws.readyState !== 1) return;
              try {
                ws.send(JSON.stringify(event));
              } catch {
                // The client went away between the readyState check and send.
              }
            }),
          );
        },
        onClose: (event, ws) => {
          this.unsubscribe(unsubscribers, ws);
          release();
          logger.debug("Live ladder WebSocket closed", {
            code: event.code,
            reason: event.reason,
            active: this.limiter.count,
          });
        },
        onError: (_event, ws) => {
          this.unsubscribe(unsubscribers, ws);
          release();
          logger.debug("Live ladder WebSocket error", { active: this.limiter.count });
        },
      }));

      return (await upgrade(c, async () => undefined)) as Response;
    });

    return router;
  }

  private unsubscribe(unsubscribers: WeakMap<object, () => void>, ws: object) {
    const unsubscribe = unsubscribers.get(ws);
    if (unsubscribe === undefined) return;
    unsubscribers.delete(ws);
    unsubscribe();
  }

  // A misconfigured proxy turns every reconnect (2s apart) into a plain GET. The request log is
  // debug-only in production, so shout once in a while instead of staying silent.
  private logMissingUpgrade(c: Context) {
    this.suppressedMissingUpgrades++;
    const now = Date.now();
    if (now - this.lastMissingUpgradeLogAt < 10_000) return;

    logger.warn("Live ladder WebSocket request is missing the Upgrade header", {
      upgrade: c.req.header("upgrade") ?? null,
      connection: c.req.header("connection") ?? null,
      forwardedFor: c.req.header("x-forwarded-for") ?? null,
      host: c.req.header("host") ?? null,
      suppressed: this.suppressedMissingUpgrades,
      hint: "Forward the Upgrade and Connection headers through the reverse proxy.",
    });
    this.lastMissingUpgradeLogAt = now;
    this.suppressedMissingUpgrades = 0;
  }

  // An attacker hammering the endpoint should not be able to flood the log.
  private logRejection() {
    this.suppressedRejections++;
    const now = Date.now();
    if (now - this.lastRejectLogAt < 5_000) return;

    logger.warn("Rejected live ladder WebSocket: connection limit reached", {
      limit: LIVE_LADDER_MAX_CONNECTIONS,
      rejected: this.suppressedRejections,
      active: this.limiter.count,
    });
    this.lastRejectLogAt = now;
    this.suppressedRejections = 0;
  }
}