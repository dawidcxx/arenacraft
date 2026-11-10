#!/usr/bin/env bun
/**
 * reset_soloq - reset the whole SoloQ ladder.
 *
 * Every SoloQ team (the otherwise-unused 5v5 arena track) gets its team rating
 * and every member's personal rating set to 1400, every member's matchmaker
 * rating (MMR) plus its high-water mark reset to 1500, and all win/loss counters
 * (season and week) zeroed out.
 *
 * Tournament seats from the auth database's account_seat table raise the
 * starting MMR for the linked account's SoloQ characters: Gold gets base + 110
 * (1610 by default) and Diamond base + 150 (1650 by default). A missing row is
 * the default None seat.
 *
 *   ./scripts/reset_soloq --dry-run
 *   ./scripts/reset_soloq --yes
 *
 * Interactive confirmation is required unless --yes is passed.
 *
 * Connects to the characters database via AC_CHARACTER_DATABASE_INFO and to the
 * auth database via AC_LOGIN_DATABASE_INFO (same "host;port;user;password;db"
 * format the core uses); both default to the local acore_* databases. Bun loads
 * .env automatically when run from the repo root.
 *
 * Seats only apply to characters that already have a SoloQ team when this runs.
 * `scripts/bracket reset --bracket soloq` also resets MMR, but ignores seats.
 *
 * The worldserver keeps arena teams in memory, so restart it after running
 * this (e.g. `./scripts/restart_world`) or online players will save their old
 * ratings back over the reset.
 */
import { Command } from "commander";
import { connect, parseDbInfo, type DbInfo } from "./lib/db.ts";
import { confirmDestructive, describeTarget } from "./lib/safety.ts";

const SOLOQ_ARENA_TYPE = 5; // ARENA_TYPE_5v5
const SOLOQ_ARENA_SLOT = 2; // ARENA_SLOT_5v5
const DEFAULT_RATING = 1400;
const DEFAULT_MMR = 1500;

// Starting MMR advantage over the None seat. At the default base (1500) this is
// the Gold 1610 / Diamond 1650 seeding agreed for tournament resets.
const SEAT_MMR_OFFSET = { Gold: 110, Diamond: 150 } as const;
type SeatTier = keyof typeof SEAT_MMR_OFFSET;
const SEAT_TIERS = Object.keys(SEAT_MMR_OFFSET) as SeatTier[];

const DEFAULT_CHARACTERS: DbInfo = {
  hostname: "127.0.0.1",
  port: 3306,
  username: "acore",
  password: "acore",
  database: "acore_characters",
};

const DEFAULT_AUTH: DbInfo = {
  ...DEFAULT_CHARACTERS,
  database: "acore_auth",
};

interface Options {
  characters: string | undefined;
  auth: string | undefined;
  rating: string;
  mmr: string;
  dryRun: boolean;
  yes: boolean;
}

const program = new Command();
program
  .name("reset_soloq")
  .description("Reset all SoloQ ratings and MMR.")
  .option("--characters <info>", "characters connection 'host;port;user;password;db' [AC_CHARACTER_DATABASE_INFO]")
  .option("--auth <info>", "auth connection for tournament seats 'host;port;user;password;db' [AC_LOGIN_DATABASE_INFO]")
  .option("--rating <value>", "team and personal rating to set", String(DEFAULT_RATING))
  .option("--mmr <value>", "matchmaker rating to set", String(DEFAULT_MMR))
  .option("--dry-run", "show what would be reset without changing anything")
  .option("--yes", "skip the interactive confirmation")
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
const seatMmr = {
  Gold: mmr + SEAT_MMR_OFFSET.Gold,
  Diamond: mmr + SEAT_MMR_OFFSET.Diamond,
};
if (seatMmr.Diamond > 65535) {
  console.error(`--mmr '${opts.mmr}' leaves no room for the Diamond seat (${seatMmr.Diamond} > 65535)`);
  process.exit(1);
}

const info = parseDbInfo(opts.characters ?? process.env.AC_CHARACTER_DATABASE_INFO, DEFAULT_CHARACTERS);
const authInfo = parseDbInfo(opts.auth ?? process.env.AC_LOGIN_DATABASE_INFO, DEFAULT_AUTH);
const sql = connect(info);
const authSql = connect(authInfo);

let seatRows: { accountId: number; tier: string }[];
try {
  seatRows = (await authSql`SELECT account_id AS accountId, tier FROM account_seat`) as {
    accountId: number;
    tier: string;
  }[];
} catch (error) {
  if ((error as { code?: string }).code === "ER_NO_SUCH_TABLE") {
    console.error("account_seat is missing from the auth database; run `ac db-update` first");
  } else {
    console.error(`failed to read tournament seats: ${String(error)}`);
  }
  await authSql.end();
  await sql.end();
  process.exit(1);
}

const seatAccounts: Record<SeatTier, number[]> = { Gold: [], Diamond: [] };
for (const row of seatRows) {
  const tier = String(row.tier);
  if (tier === "Gold" || tier === "Diamond") seatAccounts[tier].push(Number(row.accountId));
}

const teamRows = await sql`SELECT COUNT(*) AS n FROM arena_team WHERE type = ${SOLOQ_ARENA_TYPE}`;
const memberRows = await sql`
  SELECT COUNT(*) AS n
  FROM arena_team_member atm
  JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
  WHERE at.type = ${SOLOQ_ARENA_TYPE}`;
const coveredRows = await sql`
  SELECT c.account AS accountId
  FROM arena_team_member atm
  JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
  JOIN characters c ON c.guid = atm.guid
  WHERE at.type = ${SOLOQ_ARENA_TYPE}
  GROUP BY c.account`;
const teamCount = Number((teamRows[0] as { n: number }).n);
const memberCount = Number((memberRows[0] as { n: number }).n);
const coveredAccounts = new Set(coveredRows.map((row: { accountId: number }) => Number(row.accountId)));

console.log(`[characters] target ${describeTarget(info)}`);
console.log(`[auth] target ${describeTarget(authInfo)}`);
console.log(
  `[characters] ${teamCount} SoloQ team(s), ${memberCount} member(s) -> rating ${rating}, MMR ${mmr}, stats zeroed`,
);
console.log(
  `[auth] ${seatRows.length} stored seat(s): Gold ${seatAccounts.Gold.length}, Diamond ${seatAccounts.Diamond.length}`,
);
console.log(`[auth] seat starting MMR: Gold ${seatMmr.Gold}, Diamond ${seatMmr.Diamond}`);

for (const tier of SEAT_TIERS) {
  const missing = seatAccounts[tier].filter((accountId) => !coveredAccounts.has(accountId));
  if (missing.length > 0) {
    console.log(`[auth] ${missing.length} ${tier} seat(s) have no SoloQ team yet and will miss this reset`);
  }
}

const mode = await confirmDestructive({
  action: "reset SoloQ ratings on",
  target: describeTarget(info),
  yes: opts.yes,
  dryRun: opts.dryRun,
  allowDryRun: true,
});

if (mode === "dry-run") {
  console.log("[characters] dry run, nothing changed");
  await authSql.end();
  await sql.end();
  process.exit(0);
}

await sql.begin(async (tx) => {
  await tx.unsafe(
    `UPDATE arena_team
     SET rating = ?, seasonGames = 0, seasonWins = 0, weekGames = 0, weekWins = 0
     WHERE type = ?`,
    [rating, SOLOQ_ARENA_TYPE],
  );

  await tx.unsafe(
    `UPDATE arena_team_member atm
     JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
     SET atm.personalRating = ?,
         atm.seasonGames = 0, atm.seasonWins = 0,
         atm.weekGames = 0, atm.weekWins = 0
     WHERE at.type = ?`,
    [rating, SOLOQ_ARENA_TYPE],
  );

  await tx.unsafe(
    `INSERT INTO character_arena_stats (guid, slot, matchMakerRating, maxMMR)
     SELECT atm.guid, ?, ?, ?
     FROM arena_team_member atm
     JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId
     WHERE at.type = ?
     ON DUPLICATE KEY UPDATE matchMakerRating = ?, maxMMR = ?`,
    [SOLOQ_ARENA_SLOT, mmr, mmr, SOLOQ_ARENA_TYPE, mmr, mmr],
  );

  for (const tier of SEAT_TIERS) {
    const accountIds = seatAccounts[tier];
    if (accountIds.length === 0) continue;

    const placeholders = accountIds.map(() => "?").join(", ");
    await tx.unsafe(
      `UPDATE character_arena_stats cas
       JOIN characters c ON c.guid = cas.guid
       SET cas.matchMakerRating = ?, cas.maxMMR = ?
       WHERE cas.slot = ? AND c.account IN (${placeholders})`,
      [seatMmr[tier], seatMmr[tier], SOLOQ_ARENA_SLOT, ...accountIds],
    );
  }
});

console.log(
  `[characters] reset ${teamCount} SoloQ team(s) and ${memberCount} member(s) to rating ${rating}, MMR ${mmr}, stats zeroed`,
);
console.log(
  `[characters] applied seats: Gold ${seatAccounts.Gold.length} account(s) at ${seatMmr.Gold}, Diamond ${seatAccounts.Diamond.length} account(s) at ${seatMmr.Diamond}`,
);
console.log("[characters] restart the worldserver so it drops the in-memory ratings");

await authSql.end();
await sql.end();
