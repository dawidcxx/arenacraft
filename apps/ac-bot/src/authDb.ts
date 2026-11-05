import mysql from "mysql2/promise";
import type { Pool, ResultSetHeader, RowDataPacket } from "mysql2/promise";

import { makeRegistrationData } from "./srp6";

const EXPANSION_WOTLK = 2;

export interface AccountRef {
  id: number;
  username: string;
}

export interface AccountSummary {
  id: number;
  username: string;
  discordId: string | null;
  gmLevel: number;
}

export class AuthDb {
  private readonly pool: Pool;

  constructor(url: string) {
    this.pool = mysql.createPool(url);
  }

  async close(): Promise<void> {
    await this.pool.end();
  }

  async assertSchema(): Promise<void> {
    try {
      await this.pool.query("SELECT 1 FROM account_discord LIMIT 1");
    } catch (error) {
      if ((error as { code?: string }).code === "ER_NO_SUCH_TABLE") {
        throw new Error("account_discord table is missing; run `ac db-update` to apply data/sql updates");
      }
      throw error;
    }
  }

  /** Resolves account ids to usernames. Missing ids are simply absent from the map. */
  async accountNames(ids: number[]): Promise<Map<number, string>> {
    const unique = [...new Set(ids)];
    if (unique.length === 0) return new Map();

    const placeholders = unique.map(() => "?").join(", ");
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT id, username FROM account WHERE id IN (${placeholders})`,
      unique,
    );
    return new Map(rows.map((row) => [Number(row.id), String(row.username)]));
  }

  /** All account ids owned by a Discord user, ordered by account id. */
  async accountIdsByDiscordId(discordId: string): Promise<number[]> {
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT account_id AS accountId
       FROM account_discord
       WHERE discord_id = ?
       ORDER BY account_id ASC`,
      [discordId],
    );
    return rows.map((row) => Number(row.accountId));
  }

  async accountByDiscordId(discordId: string): Promise<AccountRef | null> {
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT a.id, a.username
       FROM account a
       JOIN account_discord d ON d.account_id = a.id
       WHERE d.discord_id = ?
       LIMIT 1`,
      [discordId],
    );
    const row = rows[0];
    return row ? { id: Number(row.id), username: String(row.username) } : null;
  }

  async discordIdsByAccountIds(ids: number[]): Promise<Map<number, string>> {
    const unique = [...new Set(ids)];
    if (unique.length === 0) return new Map();

    const placeholders = unique.map(() => "?").join(", ");
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT account_id AS accountId, discord_id AS discordId
       FROM account_discord
       WHERE account_id IN (${placeholders})`,
      unique,
    );
    return new Map(rows.map((row) => [Number(row.accountId), String(row.discordId)]));
  }

  /**
   * Creates a fresh account owned by a Discord user. The caller must have
   * verified the account name and Discord ID are not already used.
   */
  async createAccount(discordId: string, username: string, password: string): Promise<void> {
    const { salt, verifier } = makeRegistrationData(username, password);

    const connection = await this.pool.getConnection();
    try {
      await connection.beginTransaction();
      const [inserted] = await connection.execute<ResultSetHeader>(
        "INSERT INTO account (username, salt, verifier, expansion, joindate) VALUES (?, ?, ?, ?, NOW())",
        [username, salt, verifier, EXPANSION_WOTLK],
      );
      const accountId = inserted.insertId;

      await connection.execute("INSERT INTO account_discord (account_id, discord_id) VALUES (?, ?)", [
        accountId,
        discordId,
      ]);

      // Mirror the core's LOGIN_INS_REALM_CHARACTERS_INIT: one zero-count row
      // per realm so the realm list shows the account.
      await connection.execute(
        `INSERT INTO realmcharacters (realmid, acctid, numchars)
         SELECT r.id, ?, 0
         FROM realmlist r
         LEFT JOIN realmcharacters rc ON rc.realmid = r.id AND rc.acctid = ?
         WHERE rc.acctid IS NULL`,
        [accountId, accountId],
      );
      await connection.commit();
    } catch (error) {
      await connection.rollback();
      throw error;
    } finally {
      connection.release();
    }
  }

  /**
   * Replaces an existing account's credentials.
   */
  async resetPasswordForAccount(accountId: number, password: string): Promise<void> {
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      "SELECT username FROM account WHERE id = ? LIMIT 1",
      [accountId],
    );
    const row = rows[0];
    if (!row) throw new Error(`Account ${accountId} does not exist`);

    const { salt, verifier } = makeRegistrationData(String(row.username), password);
    await this.pool.execute("UPDATE account SET salt = ?, verifier = ? WHERE id = ?", [
      salt,
      verifier,
      accountId,
    ]);
  }

  async listAccounts(): Promise<AccountSummary[]> {
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT a.id, a.username, d.discord_id AS discordId,
              COALESCE((SELECT MAX(aa.gmlevel) FROM account_access aa WHERE aa.id = a.id), 0) AS gmLevel
       FROM account a
       LEFT JOIN account_discord d ON d.account_id = a.id
       ORDER BY a.id ASC`,
    );
    return rows.map((row) => ({
      id: Number(row.id),
      username: String(row.username),
      discordId: row.discordId === null ? null : String(row.discordId),
      gmLevel: Number(row.gmLevel),
    }));
  }

  /** Inserts or replaces the account's display alias. */
  async setAlias(accountId: number, alias: string): Promise<void> {
    await this.pool.execute(
      "INSERT INTO account_alias (account_id, alias) VALUES (?, ?) ON DUPLICATE KEY UPDATE alias = ?",
      [accountId, alias, alias],
    );
  }

  /** Deletes the account's alias. Returns false when there was nothing to delete. */
  async resetAlias(accountId: number): Promise<boolean> {
    const [result] = await this.pool.execute<ResultSetHeader>("DELETE FROM account_alias WHERE account_id = ?", [
      accountId,
    ]);
    return result.affectedRows > 0;
  }
}
