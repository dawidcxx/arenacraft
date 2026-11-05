import { getConfig } from "./config";
import { toLadderEntry } from "./ladder";
import { LadderDb } from "./ladderDb";
import { logger } from "./logger";
import { createHandler } from "./server";
import { SnapshotCache } from "./snapshotCache";

const config = getConfig();
const db = new LadderDb(config.CHARACTER_DB_URL, config.DB_TIMEOUT_MS);
const cache = new SnapshotCache(config.CACHE_TTL_SECONDS * 1000, async () => {
  try {
    return (await db.topSoloq(config.LADDER_SIZE)).map(toLadderEntry);
  } catch (error) {
    // the cache keeps serving the previous snapshot, so this is the only trace of an outage
    logger.error("ladder refresh failed", error);
    throw error;
  }
});

const server = Bun.serve({
  hostname: config.HOST,
  port: config.PORT,
  // GET-only API: refuse any request body and drop idle keep-alive connections quickly.
  maxRequestBodySize: 1024,
  idleTimeout: 10,
  fetch: createHandler({
    ladder: () => cache.get(),
    corsOrigin: config.CORS_ORIGIN,
    cacheTtlSeconds: config.CACHE_TTL_SECONDS,
    authTokens: config.AUTH_TOKENS,
  }),
  error(error) {
    logger.error("unhandled request error", error);
    return new Response(JSON.stringify({ error: "internal error" }), {
      status: 500,
      headers: { "Content-Type": "application/json; charset=utf-8" },
    });
  },
});

logger.info(`api listening on ${server.hostname}:${server.port}`);

async function shutdown(signal: string): Promise<void> {
  logger.info(`${signal} received, shutting down`);
  server.stop();
  await db.close();
  process.exit(0);
}

process.on("SIGINT", () => void shutdown("SIGINT"));
process.on("SIGTERM", () => void shutdown("SIGTERM"));
