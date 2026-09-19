import { Redis } from "ioredis";

import { logger } from "./logger";
import { MATCHUP_CHANNEL, MATCHUP_ENDED_CHANNEL, MatchupEndedEventSchema, MatchupEventSchema } from "./types";
import type { RoomManager } from "./voiceRooms";

export async function startEventSubscriber(redisUrl: string, rooms: RoomManager): Promise<Redis> {
  const redis = new Redis(redisUrl);
  redis.on("error", (error) => logger.error("Redis error", error));
  redis.on("message", (channel, message) => {
    void handleMessage(channel, message, rooms);
  });

  await redis.subscribe(MATCHUP_CHANNEL, MATCHUP_ENDED_CHANNEL);
  logger.info(`Subscribed to ${MATCHUP_CHANNEL} and ${MATCHUP_ENDED_CHANNEL}`);
  return redis;
}

async function handleMessage(channel: string, message: string, rooms: RoomManager): Promise<void> {
  try {
    const raw: unknown = JSON.parse(message);

    if (channel === MATCHUP_CHANNEL) {
      const parsed = MatchupEventSchema.safeParse(raw);
      if (!parsed.success) {
        logger.warn("Ignoring malformed matchup event", parsed.error.issues);
        return;
      }
      const event = parsed.data;
      await rooms.book(
        event.instanceId,
        event.teamA.players.map((player) => player.accountName),
        event.teamB.players.map((player) => player.accountName),
      );
      return;
    }

    if (channel === MATCHUP_ENDED_CHANNEL) {
      const parsed = MatchupEndedEventSchema.safeParse(raw);
      if (!parsed.success) {
        logger.warn("Ignoring malformed matchup-ended event", parsed.error.issues);
        return;
      }
      const event = parsed.data;
      await rooms.release(
        event.instanceId,
        event.players.map((player) => player.accountName),
      );
    }
  } catch (error) {
    logger.error(`Failed to handle Redis message on '${channel}'`, error);
  }
}
