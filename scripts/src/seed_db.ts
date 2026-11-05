#!/usr/bin/env bun
/**
 * seed_db - one-time development bootstrap of the acore databases.
 *
 * Creates the databases and the `acore` application user via an admin
 * connection, then applies every base SQL file from data/sql/base. Pending
 * updates are intentionally not applied; run `ac db-update` for those.
 *
 * Usage:
 *   ./scripts/seed_db mysql://root:acore@localhost:3306
 *   ./scripts/seed_db 'localhost;3306;root;acore'
 *   ./scripts/seed_db 'root;acore'                 (localhost:3306)
 *
 * No AC_* environment variables are read.
 */
import { join } from "node:path";
import { readdir } from "node:fs/promises";
import { connect, splitStatements, type DbInfo } from "./lib/db.ts";
import { repoRoot } from "./lib/repo.ts";

const DBS = ["auth", "characters", "world"] as const;
type DbName = (typeof DBS)[number];
const DIR_FOR_DB: Record<DbName, string> = { auth: "db_auth", characters: "db_characters", world: "db_world" };
const DATABASE_FOR_DB: Record<DbName, string> = {
  auth: "acore_auth",
  characters: "acore_characters",
  world: "acore_world",
};

const USAGE = `usage: seed_db <admin-connection>

  admin-connection:
    mysql://user:password@host:port
    host;port;user;password
    user;password                  (uses localhost:3306)

example:
  ./scripts/seed_db mysql://root:acore@localhost:3306`;

function fail(message: string): never {
  console.error(message);
  process.exit(1);
}

function parseAdminConnection(value: string): DbInfo {
  if (value.startsWith("mysql://")) {
    let url: URL;
    try {
      url = new URL(value);
    } catch {
      fail(`invalid connection URL: ${value}`);
    }

    if (!url.username)
      fail(`missing username in connection URL: ${value}`);

    return {
      hostname: url.hostname || "127.0.0.1",
      port: Number(url.port) || 3306,
      username: decodeURIComponent(url.username),
      password: decodeURIComponent(url.password),
    };
  }

  const parts = value.split(";").map((part) => part.trim());

  if (parts.length === 2 && parts[0]) {
    return { hostname: "127.0.0.1", port: 3306, username: parts[0], password: parts[1] ?? "" };
  }

  if (parts.length === 4 && parts[2]) {
    return {
      hostname: parts[0] || "127.0.0.1",
      port: Number(parts[1]) || 3306,
      username: parts[2],
      password: parts[3] ?? "",
    };
  }

  fail(`invalid connection string: ${value}\n\n${USAGE}`);
}

async function listSqlFiles(dir: string): Promise<string[]> {
  try {
    const entries = await readdir(dir);
    const files = entries.filter((file) => file.endsWith(".sql")).sort();

    // `updates.sql` doubles as the "already seeded" marker, so apply it only
    // after every other base file. A failed seed then still looks unseeded.
    files.sort((a, b) => (a === "updates.sql" ? 1 : b === "updates.sql" ? -1 : 0));
    return files;
  } catch {
    return [];
  }
}

async function hasUpdatesTable(info: DbInfo): Promise<boolean> {
  const sql = connect(info);
  const rows = await sql`SELECT table_name AS name FROM information_schema.tables
    WHERE table_schema = ${info.database} AND table_name = 'updates'`;
  sql.end();
  return rows.length > 0;
}

async function applyCreateScript(info: DbInfo): Promise<void> {
  const sql = connect(info);
  const path = join(repoRoot, "data", "sql", "create", "create_mysql.sql");
  const text = await Bun.file(path).text();

  for (const statement of splitStatements(text))
    await sql.unsafe(statement);

  sql.end();
}

async function seedDatabase(name: DbName): Promise<void> {
  const label = `[${name}]`;
  const info = { ...admin, database: DATABASE_FOR_DB[name] };

  if (await hasUpdatesTable(info)) {
    console.log(`${label} already seeded, skipping base`);
    return;
  }

  const baseDir = join(repoRoot, "data", "sql", "base", DIR_FOR_DB[name]);
  const files = await listSqlFiles(baseDir);
  if (files.length === 0)
    fail(`${label} no base SQL files found in ${baseDir}`);

  const sql = connect(info);
  let statements = 0;

  for (const file of files) {
    const text = await Bun.file(join(baseDir, file)).text();
    const split = splitStatements(text);
    for (const statement of split)
      await sql.unsafe(statement);
    statements += split.length;
    console.log(`${label} applied base/${file} (${split.length} statements)`);
  }

  sql.end();
  console.log(`${label} seeded ${files.length} base files (${statements} statements)`);
}

const args = process.argv.slice(2);
const connectionArg = args[0];
if (args.length !== 1 || !connectionArg || connectionArg === "-h" || connectionArg === "--help") {
  console.log(USAGE);
  process.exit(connectionArg === "-h" || connectionArg === "--help" ? 0 : 1);
}

const admin = parseAdminConnection(connectionArg);

console.log("creating databases and the acore user");
await applyCreateScript(admin);

for (const name of DBS)
  await seedDatabase(name);

console.log("done; start the servers or run: ac db-update");
