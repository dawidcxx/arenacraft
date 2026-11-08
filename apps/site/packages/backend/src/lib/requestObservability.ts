import { randomUUID } from "node:crypto";
import type { MiddlewareHandler } from "hono";
import { routePath } from "hono/route";
import * as v from "valibot";
import { getClientIP } from "./honoJsUtils";
import { Logger } from "./Logger";

export const REQUEST_ID_HEADER = "x-request-id";

const logger = Logger.for("http");

const RequestId = v.pipe(v.string(), v.uuid());

export const requestObservability = (): MiddlewareHandler => {
  return async (c, next) => {
    const requestId = resolveRequestId(c.req.header(REQUEST_ID_HEADER));
    c.set("requestId", requestId);
    c.header(REQUEST_ID_HEADER, requestId);

    const startedAt = performance.now();
    await next();
    const durationMs = Math.round(performance.now() - startedAt);

    logger.debug("Request handled", {
      requestId,
      method: c.req.method,
      path: c.req.path,
      status: c.res.status,
      durationMs,
      ip: getClientIP(c),
    });

    logger.metric(`${c.req.method} ${routePath(c)}`, durationMs, "ms");
  };
};

function resolveRequestId(incoming: string | undefined): string {
  if (incoming !== undefined) {
    const parsed = v.safeParse(RequestId, incoming);
    if (parsed.success) {
      return parsed.output;
    }
  }
  return randomUUID();
}