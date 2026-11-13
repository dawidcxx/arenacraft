import { HttpStatusCodes } from "@arenacraft/common";
import { upgradeWebSocket, type HttpBindings } from "@hono/node-server";
import { inject, injectable } from "@needle-di/core";
import { Hono } from "hono";
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

  constructor(
    private readonly service: ILadderTopService = inject(LADDER_TOP_SERVICE),
    private readonly limiter: WebSocketConnectionLimiter = inject(WebSocketConnectionLimiter),
  ) {}

  build() {
    const router = new Hono();
    const unsubscribers = new WeakMap<object, () => void>();

    router.get("/ws", async (c): Promise<Response> => {
      if (c.req.header("upgrade")?.toLowerCase() !== "websocket") {
        return c.json(
          { code: "UpgradeRequired", message: "This endpoint only speaks WebSocket" },
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
        onClose: (_event, ws) => {
          this.unsubscribe(unsubscribers, ws);
          release();
        },
        onError: (_event, ws) => {
          this.unsubscribe(unsubscribers, ws);
          release();
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