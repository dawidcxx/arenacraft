import { createApp } from "./app";
import { SnapshotCache } from "./cache/snapshotCache";
import { getConfig } from "./config";
import { createDb } from "./db/client";
import { LadderRepository } from "./db/ladderRepository";
import { toLadderEntry } from "./ladder/ladder";
import { logger } from "./logger";

const config = getConfig();
const { db, close } = createDb(config.CHARACTER_DB_URL, config.DB_TIMEOUT_MS);
const ladderRepository = new LadderRepository(db, config.DB_TIMEOUT_MS);

const cache = new SnapshotCache(config.CACHE_TTL_SECONDS * 1000, async () => {
  try {
    return (await ladderRepository.topSoloq(config.LADDER_SIZE)).map(toLadderEntry);
  } catch (error) {
    // the cache keeps serving the previous snapshot, so this is the only trace of an outage
    logger.error("ladder refresh failed", error);
    throw error;
  }
});

const app = createApp({
  ladder: () => cache.get(),
  corsOrigin: config.CORS_ORIGIN,
  cacheTtlSeconds: config.CACHE_TTL_SECONDS,
  authTokens: config.AUTH_TOKENS,
});

const server = Bun.serve({
  hostname: config.HOST,
  port: config.PORT,
  // GET-only API: refuse any request body and drop idle keep-alive connections quickly.
  maxRequestBodySize: 1024,
  idleTimeout: 10,
  fetch: app.fetch,
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
  await close();
  process.exit(0);
}

process.on("SIGINT", () => void shutdown("SIGINT"));
process.on("SIGTERM", () => void shutdown("SIGTERM"));
