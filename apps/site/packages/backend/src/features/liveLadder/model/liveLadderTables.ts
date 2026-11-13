import { int, mysqlTable, varchar } from "drizzle-orm/mysql-core";

// Lives in the auth database (`data/sql/updates/db_auth/2026_11_01_00.sql`). The Discord bot
// writes it, the in-game `.alias get` command reads it.
export const accountAlias = mysqlTable("account_alias", {
  accountId: int("account_id", { unsigned: true }).primaryKey(),
  alias: varchar("alias", { length: 32 }).notNull(),
});