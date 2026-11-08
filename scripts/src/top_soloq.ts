#!/usr/bin/env bun
/**
 * top_soloq - watch the top of the SoloQ ladder in the terminal.
 *
 * Queries the characters database directly (read-only) and re-renders the best
 * players by personal rating, once per second by default. Handy for capturing
 * who held rank 1 at an exact moment, e.g. a season cutoff:
 *
 *   ./scripts/top_soloq                    # refresh forever until Ctrl-C
 *   ./scripts/top_soloq --limit 5          # show the top five
 *   ./scripts/top_soloq --once             # single snapshot
 *   ./scripts/top_soloq --until 21:00      # poll until 21:00, print the final standings, exit
 *
 * --until accepts HH:MM or HH:MM:SS in local time and resolves to the next
 * occurrence (today if still ahead, otherwise tomorrow). The poll that lands
 * at/after the target is the one printed as the final snapshot.
 *
 * Connects to the characters database via AC_CHARACTER_DATABASE_INFO (same
 * "host;port;user;password;db" format the core uses); defaults to the local
 * acore_characters database. Bun loads .env automatically when run from the
 * repo root.
 */
import type { SQL } from "bun";
import { Command } from "commander";
import { connect, parseDbInfo, type DbInfo } from "./lib/db.ts";

const SOLOQ_ARENA_TYPE = 5; // ARENA_TYPE_5v5 - soloq runs on the 5v5 team track

const DEFAULT_CHARACTERS: DbInfo = {
  hostname: "127.0.0.1",
  port: 3306,
  username: "acore",
  password: "acore",
  database: "acore_characters",
};

// Same shaping as apps/api/src/ladder/ladder.ts.
const CLASS_NAMES: Record<number, string> = {
  1: "Warrior",
  2: "Paladin",
  3: "Hunter",
  4: "Rogue",
  5: "Priest",
  6: "Death Knight",
  7: "Shaman",
  8: "Mage",
  9: "Warlock",
  11: "Druid",
};
const STORED_ROLES: Record<number, string> = { 0: "Melee", 1: "Caster", 2: "Healer" };
const SINGLE_ROLE_CLASSES: Record<number, string> = {
  1: "Melee",
  3: "Melee",
  4: "Melee",
  6: "Melee",
  8: "Caster",
  9: "Caster",
};

interface Options {
  characters: string | undefined;
  limit: string;
  interval: string;
  until: string | undefined;
  once: boolean;
}

interface LadderRow {
  name: string;
  classId: number;
  storedRole: number | null;
  rating: number;
  wins: number;
  games: number;
}

interface Entry {
  name: string;
  className: string;
  role: string;
  rating: number;
  wins: number;
  losses: number;
}

const program = new Command();
program
  .name("top_soloq")
  .description("Refresh the top of the SoloQ ladder in the terminal.")
  .option("--characters <info>", "characters connection 'host;port;user;password;db' [AC_CHARACTER_DATABASE_INFO]")
  .option("-n, --limit <n>", "number of players to show", "1")
  .option("-i, --interval <ms>", "refresh interval in milliseconds", "1000")
  .option("--until <time>", "stop at the next local HH:MM[:SS] and print the final standings")
  .option("--once", "fetch a single snapshot and exit")
  .parse(process.argv);

const opts = program.opts<Options>();

function fail(message: string): never {
  console.error(message);
  process.exit(1);
}

const limit = Number(opts.limit);
if (!Number.isInteger(limit) || limit < 1) fail(`invalid --limit '${opts.limit}' (expected a positive integer)`);

const interval = Number(opts.interval);
if (!Number.isFinite(interval) || interval < 100) fail(`invalid --interval '${opts.interval}' (expected >= 100 ms)`);

function roleOf(classId: number, storedRole: number | null): string {
  if (storedRole !== null && STORED_ROLES[storedRole]) return STORED_ROLES[storedRole];
  return SINGLE_ROLE_CLASSES[classId] ?? "";
}

function parseUntil(value: string): Date {
  const match = /^(\d{1,2}):(\d{2})(?::(\d{2}))?$/.exec(value.trim());
  if (!match) fail(`invalid --until '${value}' (expected HH:MM or HH:MM:SS)`);
  const [, h, m, s] = match;
  const hour = Number(h);
  const minute = Number(m);
  const second = Number(s ?? 0);
  if (hour > 23 || minute > 59 || second > 59) fail(`invalid --until '${value}' (out of range)`);

  const now = Date.now();
  const target = new Date(now);
  target.setHours(hour, minute, second, 0);
  if (target.getTime() < now) target.setDate(target.getDate() + 1);
  return target;
}

async function fetchTop(sql: SQL, count: number): Promise<Entry[]> {
  const rows = (await sql`
    SELECT c.name            AS name,
           c.class           AS classId,
           r.role            AS storedRole,
           m.personalRating  AS rating,
           m.seasonWins      AS wins,
           m.seasonGames     AS games
      FROM arena_team t
      JOIN characters c        ON c.guid = t.captainGuid
      JOIN arena_team_member m ON m.arenaTeamId = t.arenaTeamId AND m.guid = c.guid
      LEFT JOIN character_soloq_role r ON r.guid = c.guid
     WHERE t.type = ${SOLOQ_ARENA_TYPE} AND m.seasonGames > 0
     ORDER BY m.personalRating DESC, c.name ASC
     LIMIT ${count}
  `) as unknown as LadderRow[];

  return rows.map((row) => {
    const classId = Number(row.classId);
    const storedRole = row.storedRole === null || row.storedRole === undefined ? null : Number(row.storedRole);
    const wins = Number(row.wins);
    return {
      name: String(row.name),
      className: CLASS_NAMES[classId] ?? "Unknown",
      role: roleOf(classId, storedRole),
      rating: Number(row.rating),
      wins,
      losses: Math.max(0, Number(row.games) - wins),
    };
  });
}

function pad(value: string | number, width: number, side: "left" | "right" = "left"): string {
  const text = String(value);
  return side === "left" ? text.padEnd(width) : text.padStart(width);
}

function table(entries: Entry[]): string[] {
  if (!entries.length) return ["(no ranked SoloQ players with a game this season)"];
  const lines = [
    [pad("#", 3, "right"), pad("player", 20), pad("rating", 6, "right"), pad("W-L", 9, "right"), pad("class", 11), "role"].join(
      "  ",
    ),
  ];
  entries.forEach((entry, index) => {
    lines.push(
      [
        pad(index + 1, 3, "right"),
        pad(entry.name, 20),
        pad(entry.rating, 6, "right"),
        pad(`${entry.wins}-${entry.losses}`, 9, "right"),
        pad(entry.className, 11),
        entry.role || "-",
      ].join("  "),
    );
  });
  return lines;
}

function timestamp(date: Date): string {
  const p = (n: number) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${p(date.getMonth() + 1)}-${p(date.getDate())} ${p(date.getHours())}:${p(date.getMinutes())}:${p(date.getSeconds())}`;
}

function countdown(ms: number): string {
  const total = Math.max(0, Math.ceil(ms / 1000));
  const p = (n: number) => String(n).padStart(2, "0");
  return `${p(Math.floor(total / 3600))}:${p(Math.floor((total % 3600) / 60))}:${p(total % 60)}`;
}

function render(entries: Entry[], now: Date, target: Date | null, final: boolean, interactive: boolean): void {
  if (interactive) process.stdout.write("\x1b[2J\x1b[H");
  const out: string[] = [];
  if (final) out.push("=== FINAL SNAPSHOT ===");
  let title = `${timestamp(now)}  Arenacraft SoloQ ladder`;
  if (target) title += `  (T-${countdown(target.getTime() - now.getTime())})`;
  out.push(title, "", ...table(entries), "");
  process.stdout.write(out.join("\n") + "\n");
}

const target = opts.until ? parseUntil(opts.until) : null;
const interactive = Boolean(process.stdout.isTTY) && !opts.once;
const info = parseDbInfo(opts.characters ?? process.env.AC_CHARACTER_DATABASE_INFO, DEFAULT_CHARACTERS);
const sql = connect(info);

try {
  for (;;) {
    let entries: Entry[];
    try {
      entries = await fetchTop(sql, limit);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (opts.once) fail(`query failed: ${message}`);
      if (interactive) process.stdout.write("\x1b[2J\x1b[H");
      process.stdout.write(`${timestamp(new Date())}  query failed: ${message}\n`);
      await Bun.sleep(interval);
      continue;
    }

    const now = new Date();
    const reached = target !== null && now.getTime() >= target.getTime();
    render(entries, now, target, reached, interactive);

    if (opts.once || reached) break;
    await Bun.sleep(interval);
  }
} finally {
  await sql.end();
}
