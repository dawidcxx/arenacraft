#!/usr/bin/env bun
/**
 * db_sync - populates the target databases from data/sql.
 *
 * Mirrors the upstream AzerothCore database updater conventions (forward
 * only, no rollbacks):
 *   - empty database (no `updates` table) -> apply all base files from
 *     data/sql/base/db_<name>/ in alphabetical order, then record state in
 *     a fresh `updates` table
 *   - otherwise apply data/sql/updates/db_<name>/*.sql in filename order,
 *     skipping files already recorded; re-recorded files whose sha1 changed
 *     are reported and skipped (never re-applied)
 *
 * Credentials come from env variables in the same format the core expects
 * (see .env.example at the repo root), all optional:
 *   AC_ROOT_DATABASE_INFO        "host;port;user;password"   (creates dbs/users)
 *   AC_LOGIN_DATABASE_INFO       "host;port;user;password;db"   -> db_auth
 *   AC_CHARACTER_DATABASE_INFO   "host;port;user;password;db"   -> db_characters
 *   AC_WORLD_DATABASE_INFO       "host;port;user;password;db"   -> db_world
 */
import { Command } from "commander";
import { join, resolve } from "node:path";
import { readdir } from "node:fs/promises";
import {
  connect,
  parseDbInfo,
  quoteIdent,
  quoteStr,
  sha1File,
  splitStatements,
  type DbInfo,
} from "./lib/db.ts";
import { repoRoot } from "./lib/repo.ts";

const DBS = ["auth", "characters", "world"] as const;
type DbName = (typeof DBS)[number];
const DIR_FOR_DB: Record<DbName, string> = { auth: "db_auth", characters: "db_characters", world: "db_world" };
const ENV_FOR_DB: Record<DbName, string> = {
  auth: "AC_LOGIN_DATABASE_INFO",
  characters: "AC_CHARACTER_DATABASE_INFO",
  world: "AC_WORLD_DATABASE_INFO",
};
const DEFAULT_FOR_DB: Record<DbName, DbInfo> = {
  auth: { hostname: "127.0.0.1", port: 3306, username: "acore", password: "acore", database: "acore_auth" },
  characters: { hostname: "127.0.0.1", port: 3306, username: "acore", password: "acore", database: "acore_characters" },
  world: { hostname: "127.0.0.1", port: 3306, username: "acore", password: "acore", database: "acore_world" },
};
const DEFAULT_ROOT: DbInfo = { hostname: "127.0.0.1", port: 3306, username: "root", password: "acore" };

interface Options {
  root: string | undefined;
  auth: string | undefined;
  characters: string | undefined;
  world: string | undefined;
  dataDir: string;
  db: string;
  dryRun: boolean;
}

const program = new Command();
program
  .name("db_sync")
  .description("Populate the target databases from data/sql (forward only).")
  .option("--root <info>", "root connection 'host;port;user;password' [AC_ROOT_DATABASE_INFO]")
  .option("--auth <info>", "auth connection 'host;port;user;password;db' [AC_LOGIN_DATABASE_INFO]")
  .option("--characters <info>", "characters connection [AC_CHARACTER_DATABASE_INFO]")
  .option("--world <info>", "world connection [AC_WORLD_DATABASE_INFO]")
  .option("--data-dir <dir>", "sql data dir", join(repoRoot, "data", "sql"))
  .option("--db <names>", "comma separated subset of auth,characters,world", DBS.join(","))
  .option("--dry-run", "show what would be applied without executing")
  .parse(process.argv);

const opts = program.opts<Options>();
const selected = opts.db.split(",").map((s) => s.trim()).filter(Boolean) as DbName[];
for (const name of selected) {
  if (!DBS.includes(name)) {
    console.error(`unknown database '${name}' (expected one of: ${DBS.join(", ")})`);
    process.exit(1);
  }
}

const targetFor: Record<DbName, DbInfo> = {
  auth: parseDbInfo(opts.auth ?? process.env[ENV_FOR_DB.auth], DEFAULT_FOR_DB.auth),
  characters: parseDbInfo(opts.characters ?? process.env[ENV_FOR_DB.characters], DEFAULT_FOR_DB.characters),
  world: parseDbInfo(opts.world ?? process.env[ENV_FOR_DB.world], DEFAULT_FOR_DB.world),
};
const rootInfo = parseDbInfo(opts.root ?? process.env.AC_ROOT_DATABASE_INFO, DEFAULT_ROOT);
const dataDir = resolve(opts.dataDir);
const updatesTableName = "updates";

const CREATE_UPDATES_TABLE =
  `CREATE TABLE IF NOT EXISTS ${quoteIdent(updatesTableName)} (` +
  "`name` VARCHAR(30) NOT NULL COMMENT 'filename with extension of the update.'," +
  "`hash` CHAR(40) NOT NULL DEFAULT '' COMMENT 'sha1 hash of the sql file.'," +
  "`state` ENUM('RELEASED','ARCHIVED') NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if an update is released or archived.'," +
  "`timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'timestamp when the query was applied.'," +
  "`speed` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'time the query takes to apply in milliseconds.'," +
  "PRIMARY KEY (`name`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

async function listSqlFiles(dir: string): Promise<string[]> {
  try {
    const entries = await readdir(dir);
    return entries.filter((f) => f.endsWith(".sql")).sort();
  } catch {
    return [];
  }
}

/** Creates the databases and the target users via the root connection. */
async function ensureDatabases(targets: DbInfo[]): Promise<void> {
  const root = connect(rootInfo);
  const users = new Map<string, string>();
  for (const t of targets) users.set(t.username, t.password);
  for (const t of targets) {
    console.log(`[root] ensure database ${t.database}`);
    await root.unsafe(
      `CREATE DATABASE IF NOT EXISTS ${quoteIdent(t.database!)} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci`,
    );
  }
  for (const [username, password] of users) {
    console.log(`[root] ensure user '${username}'@'%'`);
    await root.unsafe(`CREATE USER IF NOT EXISTS ${quoteStr(username)}@'%' IDENTIFIED BY ${quoteStr(password)}`);
    for (const t of targets) {
      if (t.username !== username) continue;
      await root.unsafe(`GRANT ALL PRIVILEGES ON ${quoteIdent(t.database!)}.* TO ${quoteStr(username)}@'%'`);
    }
  }
  root.end();
}

interface RunResult {
  baseApplied: number;
  baseStatements: number;
  updatesApplied: number;
  updatesSkipped: number;
  hashMismatches: number;
}

async function runDatabase(name: DbName): Promise<void> {
  const info = targetFor[name];
  const sql = connect(info);
  const result: RunResult = { baseApplied: 0, baseStatements: 0, updatesApplied: 0, updatesSkipped: 0, hashMismatches: 0 };
  const label = `[${name}]`;

  const existing = await sql`SELECT table_name AS name FROM information_schema.tables WHERE table_schema = ${info.database} AND table_name = ${updatesTableName}`;
  const hasUpdates = existing.length > 0;

  if (!hasUpdates) {
    const baseDir = join(dataDir, "base", DIR_FOR_DB[name]);
    const files = await listSqlFiles(baseDir);
    for (const file of files) {
      const path = join(baseDir, file);
      const text = await Bun.file(path).text();
      const statements = splitStatements(text);
      if (opts.dryRun) {
        console.log(`${label} base ${file} (${statements.length} statements) [dry run]`);
      } else {
        for (const stmt of statements) await sql.unsafe(stmt);
      }
      result.baseApplied++;
      result.baseStatements += statements.length;
    }
    if (opts.dryRun) {
      console.log(`${label} create table ${updatesTableName} [dry run]`);
    } else {
      await sql.unsafe(CREATE_UPDATES_TABLE);
    }
  }

  const updateDir = join(dataDir, "updates", DIR_FOR_DB[name]);
  const applied = new Map<string, string>();
  if (!opts.dryRun || hasUpdates) {
    // note: the base dumps ship a pre-populated `updates` table (updates.sql)
    // with rows for updates consolidated upstream - ARCHIVED entries are
    // ignored entirely, RELEASED entries with a changed sha1 are reported
    // and skipped (never re-applied), matching the upstream updater
    const rows = await sql.unsafe(
      `SELECT name, hash, state FROM ${quoteIdent(updatesTableName)}`,
    );
    for (const row of rows as { name: string; hash: string; state: string }[]) {
      applied.set(row.name, row.state === "ARCHIVED" ? "archived" : row.hash);
    }
  }

  for (const file of await listSqlFiles(updateDir)) {
    const recorded = applied.get(file);
    if (recorded !== undefined) {
      if (recorded !== "archived") {
        // recorded hashes come from mysql's SHA1() (uppercase) or previous
        // runs (lowercase) - compare case-insensitively
        const hash = (await sha1File(join(updateDir, file))).toLowerCase();
        if (hash !== recorded.toLowerCase()) {
          result.hashMismatches++;
          console.warn(`${label} WARN ${file}: sha1 changed since it was applied, skipping`);
        }
      }
      result.updatesSkipped++;
      continue;
    }
    const path = join(updateDir, file);
    const text = await Bun.file(path).text();
    const statements = splitStatements(text);
    const t0 = performance.now();
    if (opts.dryRun) {
      console.log(`${label} update ${file} (${statements.length} statements) [dry run]`);
    } else {
      for (const stmt of statements) await sql.unsafe(stmt);
      const speed = Math.round(performance.now() - t0);
      await sql.unsafe(
        `INSERT INTO ${quoteIdent(updatesTableName)} (name, hash, state, speed) VALUES (?, ?, 'RELEASED', ?)`,
        [file, await sha1File(path), speed],
      );
      console.log(`${label} applied ${file} (${statements.length} statements, ${speed}ms)`);
    }
    result.updatesApplied++;
  }

  if (opts.dryRun) {
    console.log(
      `${label} dry run done: ${result.baseApplied} base files (${result.baseStatements} statements), ` +
        `${result.updatesApplied} updates to apply, ${result.updatesSkipped} already recorded`,
    );
  } else {
    console.log(
      `${label} done: ${result.baseApplied} base files (${result.baseStatements} statements), ` +
        `${result.updatesApplied} updates applied, ${result.updatesSkipped} recorded` +
        (result.hashMismatches ? `, ${result.hashMismatches} hash mismatches` : ""),
    );
  }
  sql.end();
}

const t0 = performance.now();
// always ensure databases/users - the target connection cannot read state
// (let alone apply sql) on a pristine server without it
await ensureDatabases(selected.map((n) => targetFor[n]));
for (const name of selected) await runDatabase(name);
console.log(`db_sync finished in ${Math.round(performance.now() - t0)}ms`);
