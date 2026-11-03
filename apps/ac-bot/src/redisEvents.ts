import { Redis } from "ioredis";

import type { AuditLog } from "./auditLog";
import type { AuthDb } from "./authDb";
import { logger } from "./logger";
import { MATCHUP_CHANNEL, MATCHUP_ENDED_CHANNEL, MatchupEndedEventSchema, MatchupEventSchema } from "./types";
import type { Participant } from "./types";
import type { RoomManager } from "./voiceRooms";

export async function startEventSubscriber(
  redisUrl: string,
  rooms: RoomManager,
  authDb: AuthDb,
  auditLog: AuditLog | null = null,
): Promise<Redis> {
  const redis = new Redis(redisUrl);
  redis.on("error", (error) => logger.error("Redis error", error));
  redis.on("message", (channel, message) => {
    void handleMessage(channel, message, rooms, authDb, auditLog);
  });

  await redis.subscribe(MATCHUP_CHANNEL, MATCHUP_ENDED_CHANNEL);
  logger.info(`Subscribed to ${MATCHUP_CHANNEL} and ${MATCHUP_ENDED_CHANNEL}`);
  return redis;
}

async function resolveDiscordIds(authDb: AuthDb, players: Participant[]): Promise<string[]> {
  const ids = await authDb.discordIdsByAccountIds(players.map((player) => player.accountId));
  return players
    .map((player) => player.discordId ?? ids.get(player.accountId))
    .filter((id): id is string => typeof id === "string");
}

async function handleMessage(
  channel: string,
  message: string,
  rooms: RoomManager,
  authDb: AuthDb,
  auditLog: AuditLog | null,
): Promise<void> {
  try {
    const raw: unknown = JSON.parse(message);

    if (channel === MATCHUP_CHANNEL) {
      const parsed = MatchupEventSchema.safeParse(raw);
      if (!parsed.success) {
        logger.warn("Ignoring malformed matchup event", parsed.error.issues);
        return;
      }
      const event = parsed.data;
      const [teamA, teamB] = await Promise.all([
        resolveDiscordIds(authDb, event.teamA.players),
        resolveDiscordIds(authDb, event.teamB.players),
      ]);
      await rooms.book(event.instanceId, teamA, teamB);
      if (auditLog) await auditLog.matchup(event);
      return;
    }

    if (channel === MATCHUP_ENDED_CHANNEL) {
      const parsed = MatchupEndedEventSchema.safeParse(raw);
      if (!parsed.success) {
        logger.warn("Ignoring malformed matchup-ended event", parsed.error.issues);
        return;
      }
      const event = parsed.data;
      await rooms.release(event.instanceId, await resolveDiscordIds(authDb, event.players));
      if (auditLog) await auditLog.ended(event);
    }
  } catch (error) {
    logger.error(`Failed to handle Redis message on '${channel}'`, error);
  }
}
