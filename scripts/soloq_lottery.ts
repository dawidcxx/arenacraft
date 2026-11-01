#!/usr/bin/env bun
/**
 * soloq_lottery - draw a weighted SoloQ lottery winner.
 *
 * Scans every SoloQ team (the otherwise-unused 5v5 arena track) and coalesces
 * all characters onto one entry per account, so having many characters does not
 * by itself earn extra tickets. An account's weight is the total number of
 * games it played (wins + losses) across those characters: 100 games means 100
 * tickets, 50 games means 50 tickets, i.e. twice the chance to win.
 *
 * Only accounts where at least one character won `--min-wins` (default 5) games
 * count as eligible.
 *
 *   ./scripts/soloq_lottery
 *   ./scripts/soloq_lottery --dry-run
 *   ./scripts/soloq_lottery --min-wins 10
 *
 * Connects to the characters database via AC_CHARACTER_DATABASE_INFO (same
 * "host;port;user;password;db" format the core uses); defaults to the local
 * acore_characters database. Bun loads .env automatically when run from the
 * repo root. Read-only, so the worldserver does not need a restart.
 */
import { Command } from "commander";
import { connect, parseDbInfo, type DbInfo } from "./src/lib/db.ts";

const SOLOQ_ARENA_TYPE = 5; // ARENA_TYPE_5v5
const DEFAULT_MIN_WINS = 5;

const DEFAULT_CHARACTERS: DbInfo = {
  hostname: "127.0.0.1",
  port: 3306,
  username: "acore",
  password: "acore",
  database: "acore_characters",
};

interface Options {
  characters: string | undefined;
  minWins: string;
  dryRun: boolean;
}

interface Entry {
  account: number;
  games: number;
  wins: number;
  losses: number;
  characters: number;
}

const program = new Command();
program
  .name("soloq_lottery")
  .description("Draw a SoloQ lottery winner weighted by games played per account.")
  .option("--characters <info>", "characters connection 'host;port;user;password;db' [AC_CHARACTER_DATABASE_INFO]")
  .option("--min-wins <value>", "minimum wins a single character needs for its account to qualify", String(DEFAULT_MIN_WINS))
  .option("--dry-run", "show eligible accounts and their tickets without drawing a winner")
  .parse(process.argv);

const opts = program.opts<Options>();
const minWins = Number(opts.minWins);
if (!Number.isInteger(minWins) || minWins < 0) {
  console.error(`invalid --min-wins '${opts.minWins}'`);
  process.exit(1);
}

const info = parseDbInfo(opts.characters ?? process.env.AC_CHARACTER_DATABASE_INFO, DEFAULT_CHARACTERS);
const sql = connect(info);

const rows = await sql`
  SELECT c.account AS account,
         SUM(atm.seasonGames) AS games,
         SUM(atm.seasonWins) AS wins,
         COUNT(*) AS characters
  FROM arena_team_member atm
  JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
  JOIN characters c ON c.guid = atm.guid
  WHERE at.type = ${SOLOQ_ARENA_TYPE}
  GROUP BY c.account
  HAVING MAX(atm.seasonWins) >= ${minWins}
  ORDER BY games DESC`;

const entries: Entry[] = rows.map((row) => {
  const games = Number((row as { games: number | string }).games);
  const wins = Number((row as { wins: number | string }).wins);
  return {
    account: Number((row as { account: number }).account),
    games,
    wins,
    losses: games - wins,
    characters: Number((row as { characters: number | string }).characters),
  };
});

const totalTickets = entries.reduce((sum, entry) => sum + Math.max(entry.games, 0), 0);

console.log(
  `[characters] ${entries.length} eligible account(s) (single character with >=${minWins} wins), ${totalTickets} ticket(s)`,
);

if (opts.dryRun) {
  for (const entry of entries) {
    console.log(
      `  account ${entry.account}: ${entry.games} ticket(s) (${entry.wins}W/${entry.losses}L over ${entry.characters} character(s))`,
    );
  }
  console.log("[characters] dry run, no winner drawn");
  await sql.end();
  process.exit(0);
}

if (entries.length === 0) {
  console.error("[characters] no eligible accounts, nothing to draw");
  await sql.end();
  process.exit(1);
}

let roll = Math.floor(Math.random() * totalTickets);
let winner = entries[entries.length - 1]!;
for (const entry of entries) {
  roll -= Math.max(entry.games, 0);
  if (roll < 0) {
    winner = entry;
    break;
  }
}

console.log(
  `[lottery] winner: account ${winner.account} (${winner.games} ticket(s), ${winner.wins}W/${winner.losses}L over ${winner.characters} character(s))`,
);
console.log(`[lottery] odds: ${winner.games}/${totalTickets}`);

await sql.end();
