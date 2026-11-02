import { z } from "zod";

// Hard ceiling on rows served: the site never shows more than 200 (its ladderSize max).
export const MAX_LADDER_SIZE = 200;

const ConfigSchema = z.object({
  // mysql://user:password@host:3306/acore_characters - use a SELECT-only account (see README.md)
  CHARACTER_DB_URL: z.string().min(1),
  HOST: z.string().min(1).default("0.0.0.0"),
  PORT: z.coerce.number().int().min(1).max(65535).default(8080),
  // Rows served, best rating first.
  LADDER_SIZE: z.coerce.number().int().min(1).max(MAX_LADDER_SIZE).default(MAX_LADDER_SIZE),
  // How long a ladder snapshot is reused before the database is asked again.
  CACHE_TTL_SECONDS: z.coerce.number().int().min(1).max(3600).default(30),
  // Value of Access-Control-Allow-Origin. The ladder is public read-only data, so "*" is safe;
  // set the site origin (e.g. https://example.workers.dev) to keep other sites from embedding it.
  CORS_ORIGIN: z.string().min(1).default("*"),
  // Per-query timeout, so a stuck database cannot pile up requests.
  DB_TIMEOUT_MS: z.coerce.number().int().min(100).max(30000).default(3000),
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
