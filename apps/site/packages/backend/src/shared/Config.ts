import { InjectionToken } from "@needle-di/core";

export type DataSource = "dummy" | "mysql";

type BaseConfig = {
  host: string;
  port: number;
  ladderSize: number;
  cacheTtlSeconds: number;
  dbTimeoutMs: number;
  corsOrigins: string[];
  /** Auth database (account_alias). Optional even for mysql; aliases are skipped without it. */
  authDbUrl: string | null;
};

export type Config = BaseConfig &
  (
    | { dataSource: "dummy"; characterDbUrl: string | null }
    | { dataSource: "mysql"; characterDbUrl: string }
  );

export const CONFIG = new InjectionToken<Config>("Config");

export function loadConfig(env: NodeJS.ProcessEnv = process.env): Config {
  const base: BaseConfig = {
    host: env.HOST?.trim() || "0.0.0.0",
    port: intFromEnv(env, "PORT", 8080, 1, 65535),
    ladderSize: intFromEnv(env, "LADDER_SIZE", 200, 1, 200),
    cacheTtlSeconds: intFromEnv(env, "CACHE_TTL_SECONDS", 30, 1, 3600),
    dbTimeoutMs: intFromEnv(env, "DB_TIMEOUT_MS", 3000, 100, 30000),
    corsOrigins: listFromEnv(env.CORS_ORIGINS),
    authDbUrl: env.AUTH_DB_URL?.trim() || null,
  };

  const dataSource = parseDataSource(env.DATA_SOURCE);
  const characterDbUrl = env.CHARACTER_DB_URL?.trim() || null;

  if (dataSource === "mysql") {
    if (characterDbUrl === null) {
      throw new Error("CHARACTER_DB_URL is required when DATA_SOURCE=mysql");
    }
    return { ...base, dataSource, characterDbUrl };
  }

  return { ...base, dataSource, characterDbUrl };
}

function parseDataSource(value: string | undefined): DataSource {
  const normalized = value?.trim().toLowerCase();
  if (normalized === undefined || normalized === "" || normalized === "dummy") return "dummy";
  if (normalized === "mysql") return "mysql";
  throw new Error(`DATA_SOURCE must be 'dummy' or 'mysql', got '${value}'`);
}

function intFromEnv(
  env: NodeJS.ProcessEnv,
  key: string,
  fallback: number,
  min: number,
  max: number,
): number {
  const raw = env[key];
  if (raw === undefined || raw.trim() === "") return fallback;
  const parsed = Number(raw);
  if (!Number.isInteger(parsed) || parsed < min || parsed > max) {
    throw new Error(`${key} must be an integer between ${min} and ${max}, got '${raw}'`);
  }
  return parsed;
}

function listFromEnv(raw: string | undefined): string[] {
  return (raw ?? "")
    .split(",")
    .map((value) => value.trim())
    .filter(Boolean);
}