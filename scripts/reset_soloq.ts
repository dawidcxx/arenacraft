#!/usr/bin/env bun
/**
 * reset_soloq - reset the whole SoloQ ladder.
 *
 * Every SoloQ team (the otherwise-unused 5v5 arena track) gets its team rating
 * and every member's personal rating set to 1400, every member's matchmaker
 * rating (MMR) plus its high-water mark reset to 1500, and all win/loss counters
 * (season and week) zeroed out.
 *
 *   ./scripts/reset_soloq
 *   ./scripts/reset_soloq --dry-run
 *
 * Connects to the characters database via AC_CHARACTER_DATABASE_INFO (same
 * "host;port;user;password;db" format the core uses); defaults to the local
 * acore_characters database. Bun loads .env automatically when run from the
 * repo root.
 *
 * The worldserver keeps arena teams in memory, so restart it after running
 * this (e.g. `./scripts/restart_world`) or online players will save their old
 * ratings back over the reset.
 */
import { Command } from "commander";
import { connect, parseDbInfo, type DbInfo } from "./src/lib/db.ts";

const SOLOQ_ARENA_TYPE = 5; // ARENA_TYPE_5v5
const SOLOQ_ARENA_SLOT = 2; // ARENA_SLOT_5v5
const DEFAULT_RATING = 1400;
const DEFAULT_MMR = 1500;

const DEFAULT_CHARACTERS: DbInfo = {
  hostname: "127.0.0.1",
  port: 3306,
  username: "acore",
  password: "acore",
  database: "acore_characters",
};

interface Options {
  characters: string | undefined;
  rating: string;
  mmr: string;
  dryRun: boolean;
}

const program = new Command();
program
  .name("reset_soloq")
  .description("Reset all SoloQ ratings and MMR.")
  .option("--characters <info>", "characters connection 'host;port;user;password;db' [AC_CHARACTER_DATABASE_INFO]")
  .option("--rating <value>", "team and personal rating to set", String(DEFAULT_RATING))
  .option("--mmr <value>", "matchmaker rating to set", String(DEFAULT_MMR))
  .option("--dry-run", "show what would be reset without changing anything")
  .parse(process.argv);

const opts = program.opts<Options>();
const rating = Number(opts.rating);
const mmr = Number(opts.mmr);
if (!Number.isInteger(rating) || rating < 0 || rating > 65535) {
  console.error(`invalid --rating '${opts.rating}'`);
  process.exit(1);
}
if (!Number.isInteger(mmr) || mmr < 0 || mmr > 65535) {
  console.error(`invalid --mmr '${opts.mmr}'`);
  process.exit(1);
}

const info = parseDbInfo(opts.characters ?? process.env.AC_CHARACTER_DATABASE_INFO, DEFAULT_CHARACTERS);
const sql = connect(info);

const teamRows = await sql`SELECT COUNT(*) AS n FROM arena_team WHERE type = ${SOLOQ_ARENA_TYPE}`;
const memberRows = await sql`
  SELECT COUNT(*) AS n
  FROM arena_team_member atm
  JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
  WHERE at.type = ${SOLOQ_ARENA_TYPE}`;
const teamCount = Number((teamRows[0] as { n: number }).n);
const memberCount = Number((memberRows[0] as { n: number }).n);

console.log(
  `[characters] ${teamCount} SoloQ team(s), ${memberCount} member(s) -> rating ${rating}, MMR ${mmr}, stats zeroed`,
);

if (opts.dryRun) {
  console.log("[characters] dry run, nothing changed");
  await sql.end();
  process.exit(0);
}

await sql.unsafe(
  `UPDATE arena_team
   SET rating = ?, seasonGames = 0, seasonWins = 0, weekGames = 0, weekWins = 0
   WHERE type = ?`,
  [rating, SOLOQ_ARENA_TYPE],
);

await sql.unsafe(
  `UPDATE arena_team_member atm
   JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
   SET atm.personalRating = ?,
       atm.seasonGames = 0, atm.seasonWins = 0,
       atm.weekGames = 0, atm.weekWins = 0
   WHERE at.type = ?`,
  [rating, SOLOQ_ARENA_TYPE],
);

await sql.unsafe(
  `INSERT INTO character_arena_stats (guid, slot, matchMakerRating, maxMMR)
   SELECT atm.guid, ?, ?, ?
   FROM arena_team_member atm
   JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
   WHERE at.type = ?
   ON DUPLICATE KEY UPDATE matchMakerRating = ?, maxMMR = ?`,
  [SOLOQ_ARENA_SLOT, mmr, mmr, SOLOQ_ARENA_TYPE, mmr, mmr],
);

console.log(
  `[characters] reset ${teamCount} SoloQ team(s) and ${memberCount} member(s) to rating ${rating}, MMR ${mmr}, stats zeroed`,
);
console.log("[characters] restart the worldserver so it drops the in-memory ratings");

await sql.end();
