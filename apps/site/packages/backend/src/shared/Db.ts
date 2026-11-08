import { drizzle } from "drizzle-orm/mysql2";
import mysql from "mysql2/promise";

export function createDb(url: string, timeoutMs: number) {
  // Tiny pool: the snapshot cache in front of it means at most one query per TTL.
  const pool = mysql.createPool({
    uri: url,
    connectionLimit: 2,
    connectTimeout: timeoutMs,
    enableKeepAlive: true,
  });

  return {
    db: drizzle(pool, { mode: "default" }),
    close: () => pool.end(),
  };
}

export type DrizzleDb = ReturnType<typeof createDb>["db"];