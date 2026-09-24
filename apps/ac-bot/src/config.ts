import { z } from "zod";

const ConfigSchema = z.object({
  DISCORD_TOKEN: z.string().min(1),
  DISCORD_GUILD_ID: z.string().min(1),
  AUTH_DB_URL: z.string().min(1),
  CHARACTER_DB_URL: z.string().min(1),
  REDIS_URL: z.string().min(1).default("redis://127.0.0.1:6379"),
  WAITING_ROOM_NAME: z.string().min(1).default("Waiting Room"),
  GAME_CHANNEL_PATTERN: z.string().min(1).default("^game-(\\d+)-(\\d+)$"),
  INFO_CHANNEL_NAME: z.string().min(1).default("bot"),
  STALE_ROOM_MINUTES: z.coerce.number().int().positive().default(60),
});

export type Config = z.infer<typeof ConfigSchema>;

export function getConfig(env: NodeJS.ProcessEnv = process.env): Config {
  const parsed = ConfigSchema.safeParse(env);
  if (!parsed.success) {
    const problems = parsed.error.issues.map((issue) => `${issue.path.join(".")}: ${issue.message}`).join("; ");
    throw new Error(`Invalid environment: ${problems}`);
  }
  return parsed.data;
}
