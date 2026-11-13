import { char, datetime, int, mysqlTable, primaryKey, tinyint } from "drizzle-orm/mysql-core";

// Auth database (`data/sql/updates/db_auth/2026_11_13_00.sql`). The Discord bot's `/overlay`
// command writes it; the website only resolves tokens.
export const accountOverlay = mysqlTable("account_overlay", {
  accountId: int("account_id", { unsigned: true }).primaryKey(),
  token: char("token", { length: 32 }).notNull(),
  guid: int("guid", { unsigned: true }).notNull(),
});

// Characters database (`data/sql/updates/db_characters/2026_10_31_00.sql`). Arena replays are the
// match history: the core records every SoloQ arena (src/game/Arenacraft/replay).
export const arenaReplay = mysqlTable("arena_replay", {
  id: int("id", { unsigned: true }).primaryKey(),
  bracket: tinyint("bracket", { unsigned: true }).notNull(),
  winner: tinyint("winner", { unsigned: true }).notNull(),
  createdAt: datetime("created_at").notNull(),
});

export const arenaReplayPlayer = mysqlTable(
  "arena_replay_player",
  {
    replayId: int("replay_id", { unsigned: true }).notNull(),
    guid: int("guid", { unsigned: true }).notNull(),
    team: tinyint("team", { unsigned: true }).notNull(),
  },
  (table) => [primaryKey({ columns: [table.replayId, table.guid] })],
);