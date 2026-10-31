#!/usr/bin/env bun
/**
 * bracket - save, reset and restore arena bracket standings (2v2, 3v3, soloq).
 *
 * Bracket state lives in three characters-db tables:
 *   arena_team              team rating / season + week record / rank
 *   arena_team_member       personal rating / season + week record
 *   character_arena_stats   per-character matchmaker rating per slot
 *
 *   ./scripts/bracket save [name]              snapshot to var/brackets/<name>.json
 *   ./scripts/bracket list                     list saved snapshots
 *   ./scripts/bracket reset [--bracket ...] --yes
 *                                              save a snapshot, then reset ratings
 *                                              and records (teams/rosters are kept)
 *   ./scripts/bracket restore <name|file> --yes
 *                                              put a snapshot's standings back
 *
 * The worldserver keeps arena teams in memory and writes them back after every
 * game, so stop it (or restart it right after) before reset/restore - otherwise
 * the running server overwrites the change.
 *
 * Connection: AC_CHARACTER_DATABASE_INFO "host;port;user;password;db" or
 * --characters (same format and defaults as ./scripts/db_sync).
 */
import { Command } from "commander";
import { mkdir, readdir } from "node:fs/promises";
import { basename, join, resolve } from "node:path";
import type { SQL } from "bun";
import { connect, parseDbInfo, type DbInfo } from "./lib/db.ts";
import { repoRoot } from "./lib/repo.ts";

/** arena_team.type and the matching character_arena_stats.slot per bracket */
const BRACKETS = {
  "2v2": { type: 2, slot: 0 },
  "3v3": { type: 3, slot: 1 },
  soloq: { type: 5, slot: 2 }, // soloq runs on the 5v5 team slot
} as const;
type Bracket = keyof typeof BRACKETS;

/** start values the core uses for new teams (World.cpp / soloq Types.hpp) */
const START_TEAM_RATING: Record<Bracket, number> = { "2v2": 0, "3v3": 0, soloq: 1400 };
const START_PERSONAL_RATING: Record<Bracket, number> = { "2v2": 0, "3v3": 0, soloq: 1400 };
const START_MMR = 1500;

const DEFAULT_CHARACTERS: DbInfo = {
  hostname: "127.0.0.1",
  port: 3306,
  username: "acore",
  password: "acore",
  database: "acore_characters",
};
const snapshotDir = join(repoRoot, "var", "brackets");

interface Snapshot {
  savedAt: string;
  database: string;
  arena_team: Record<string, unknown>[];
  arena_team_member: Record<string, unknown>[];
  character_arena_stats: Record<string, unknown>[];
}

const program = new Command();
program
  .name("bracket")
  .description("Save, reset and restore arena bracket standings.")
  .option("--characters <info>", "characters connection 'host;port;user;password;db' [AC_CHARACTER_DATABASE_INFO]");

function db(): SQL {
  const info = parseDbInfo(program.opts().characters ?? process.env.AC_CHARACTER_DATABASE_INFO, DEFAULT_CHARACTERS);
  return connect(info);
}

function parseBrackets(value: string): Bracket[] {
  const names = value.split(",").map((s) => s.trim()).filter(Boolean);
  for (const n of names) {
    if (!(n in BRACKETS)) {
      console.error(`unknown bracket '${n}' (expected: ${Object.keys(BRACKETS).join(", ")})`);
      process.exit(1);
    }
  }
  return names as Bracket[];
}

function requireYes(yes: boolean | undefined, what: string): void {
  if (yes) return;
  console.error(`${what} rewrites live data; re-run with --yes (stop the world server first).`);
  process.exit(1);
}

async function save(sql: SQL, name?: string): Promise<string> {
  const savedAt = new Date().toISOString();
  const snapshot: Snapshot = {
    savedAt,
    database: String((await sql`SELECT DATABASE() AS db`)[0].db),
    arena_team: [...(await sql`SELECT * FROM arena_team`)],
    arena_team_member: [...(await sql`SELECT * FROM arena_team_member`)],
    character_arena_stats: [...(await sql`SELECT * FROM character_arena_stats`)],
  };
  await mkdir(snapshotDir, { recursive: true });
  const file = join(snapshotDir, `${name ?? savedAt.replaceAll(":", "-")}.json`);
  if (await Bun.file(file).exists()) {
    console.error(`snapshot ${file} already exists, pick another name`);
    process.exit(1);
  }
  await Bun.write(file, JSON.stringify(snapshot, null, 2) + "\n");
  console.log(
    `saved ${snapshot.arena_team.length} teams, ${snapshot.arena_team_member.length} members, ` +
      `${snapshot.character_arena_stats.length} mmr rows -> ${file}`,
  );
  return file;
}

program
  .command("save [name]")
  .description("snapshot all bracket standings to var/brackets/<name>.json")
  .action(async (name?: string) => {
    const sql = db();
    await save(sql, name);
    sql.end();
  });

program
  .command("list")
  .description("list saved snapshots")
  .action(async () => {
    const files = (await readdir(snapshotDir).catch(() => [] as string[])).filter((f) => f.endsWith(".json")).sort();
    if (!files.length) console.log(`no snapshots in ${snapshotDir}`);
    for (const f of files) console.log(basename(f, ".json"));
  });

program
  .command("reset")
  .description("snapshot, then reset ratings and season/week records (teams and rosters are kept)")
  .option("--bracket <names>", "comma separated subset of 2v2,3v3,soloq", Object.keys(BRACKETS).join(","))
  .option("--no-save", "skip the automatic snapshot")
  .option("--yes", "actually do it")
  .action(async (opts: { bracket: string; save: boolean; yes?: boolean }) => {
    const brackets = parseBrackets(opts.bracket);
    requireYes(opts.yes, "reset");
    const sql = db();
    if (opts.save) await save(sql, `pre-reset-${new Date().toISOString().replaceAll(":", "-")}`);
    await sql.begin(async (tx) => {
      for (const b of brackets) {
        const { type, slot } = BRACKETS[b];
        const teams = await tx`
          UPDATE arena_team
             SET rating = ${START_TEAM_RATING[b]}, seasonGames = 0, seasonWins = 0,
                 weekGames = 0, weekWins = 0, \`rank\` = 0
           WHERE type = ${type}`;
        const members = await tx`
          UPDATE arena_team_member m JOIN arena_team t ON t.arenaTeamId = m.arenaTeamId
             SET m.personalRating = ${START_PERSONAL_RATING[b]}, m.seasonGames = 0, m.seasonWins = 0,
                 m.weekGames = 0, m.weekWins = 0
           WHERE t.type = ${type}`;
        const mmr = await tx`
          UPDATE character_arena_stats SET matchMakerRating = ${START_MMR}, maxMMR = 0 WHERE slot = ${slot}`;
        console.log(`[${b}] reset ${teams.count} teams, ${members.count} members, ${mmr.count} mmr rows`);
      }
    });
    sql.end();
    console.log("done - restart the world server (./scripts/restart_world) so it reloads the standings");
  });

program
  .command("restore <snapshot>")
  .description("restore standings from a snapshot name or .json path")
  .option("--yes", "actually do it")
  .action(async (ref: string, opts: { yes?: boolean }) => {
    requireYes(opts.yes, "restore");
    const file = ref.endsWith(".json") ? resolve(ref) : join(snapshotDir, `${ref}.json`);
    const snap = (await Bun.file(file).json()) as Snapshot;
    const sql = db();
    await sql.begin(async (tx) => {
      for (const table of ["arena_team", "arena_team_member", "character_arena_stats"] as const) {
        await tx.unsafe(`DELETE FROM \`${table}\``);
        // chunk to stay well under max_allowed_packet / placeholder limits
        const rows = snap[table];
        for (let i = 0; i < rows.length; i += 500) await tx`INSERT INTO ${tx(table)} ${tx(rows.slice(i, i + 500))}`;
        console.log(`restored ${rows.length} rows into ${table}`);
      }
    });
    sql.end();
    console.log(`restored snapshot from ${snap.savedAt} - restart the world server so it reloads the standings`);
  });

await program.parseAsync(process.argv);
