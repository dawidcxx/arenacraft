import { int, mysqlTable, primaryKey, smallint, tinyint, varchar } from "drizzle-orm/mysql-core";

// Only the columns the ladder query reads. The rest of each table lives in the core schema
// (data/sql/base/db_characters).

export const characters = mysqlTable("characters", {
  guid: int("guid", { unsigned: true }).primaryKey(),
  account: int("account", { unsigned: true }).notNull(),
  name: varchar("name", { length: 12 }).notNull(),
  class: tinyint("class", { unsigned: true }).notNull(),
});

export const arenaTeam = mysqlTable("arena_team", {
  arenaTeamId: int("arenaTeamId", { unsigned: true }).primaryKey(),
  captainGuid: int("captainGuid", { unsigned: true }).notNull(),
  type: tinyint("type", { unsigned: true }).notNull(),
});

export const arenaTeamMember = mysqlTable(
  "arena_team_member",
  {
    arenaTeamId: int("arenaTeamId", { unsigned: true }).notNull(),
    guid: int("guid", { unsigned: true }).notNull(),
    seasonGames: smallint("seasonGames", { unsigned: true }).notNull(),
    seasonWins: smallint("seasonWins", { unsigned: true }).notNull(),
    personalRating: smallint("personalRating").notNull(),
  },
  (table) => [primaryKey({ columns: [table.arenaTeamId, table.guid] })],
);

export const characterSoloqRole = mysqlTable("character_soloq_role", {
  guid: int("guid", { unsigned: true }).primaryKey(),
  role: tinyint("role", { unsigned: true }).notNull(),
});