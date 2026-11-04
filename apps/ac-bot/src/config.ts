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
  // Channel for match found/finished/aborted messages; empty disables them.
  AUDIT_LOG_CHANNEL_NAME: z.string().default("audit-log"),
  // Role allowed to use /alias (and required by its runtime check).
  VIP_ROLE_NAME: z.string().min(1).default("vip"),
  STALE_ROOM_MINUTES: z.coerce.number().int().positive().default(60),
  // Realm address shown by /register ("set realmlist <REALMLIST>"); empty leaves it out.
  REALMLIST: z.string().default(""),
});

export type Config = z.infer<typeof ConfigSchema>;

export function authDbUrlFromEnv(env: NodeJS.ProcessEnv = process.env): string | null {
  const direct = env.AUTH_DB_URL?.trim();
  if (direct) return direct;

  const core = env.AC_LOGIN_DATABASE_INFO?.trim();
  if (!core) return null;

  const [host, port, user, password, database] = core.split(";");
  if (!host || !port || !user || password === undefined || !database) return null;
  return `mysql://${encodeURIComponent(user)}:${encodeURIComponent(password)}@${host}:${port}/${database}`;
}

export function getConfig(env: NodeJS.ProcessEnv = process.env): Config {
  const authDbUrl = authDbUrlFromEnv(env);
  const parsed = ConfigSchema.safeParse({ ...env, AUTH_DB_URL: authDbUrl ?? env.AUTH_DB_URL });
  if (!parsed.success) {
    const problems = parsed.error.issues.map((issue) => `${issue.path.join(".")}: ${issue.message}`).join("; ");
    throw new Error(`Invalid environment: ${problems}`);
  }
  return parsed.data;
}
