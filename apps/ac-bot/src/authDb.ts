import mysql from "mysql2/promise";
import type { Pool, RowDataPacket } from "mysql2/promise";

import { makeRegistrationData } from "./srp6";

const EXPANSION_WOTLK = 2;

export class AuthDb {
  private readonly pool: Pool;

  constructor(url: string) {
    this.pool = mysql.createPool(url);
  }

  async close(): Promise<void> {
    await this.pool.end();
  }

  async accountExists(username: string): Promise<boolean> {
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      "SELECT id FROM account WHERE username = ? LIMIT 1",
      [username],
    );
    return rows.length > 0;
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

  /**
   * Creates a fresh account. The caller must have verified the account does not
   * exist. Writes only to the auth database (account + realm character counts).
   */
  async createAccount(username: string, password: string): Promise<void> {
    const { salt, verifier } = makeRegistrationData(username, password);

    const connection = await this.pool.getConnection();
    try {
      await connection.beginTransaction();
      await connection.execute(
        "INSERT INTO account (username, salt, verifier, expansion, joindate) VALUES (?, ?, ?, ?, NOW())",
        [username, salt, verifier, EXPANSION_WOTLK],
      );
      // Mirror the core's LOGIN_INS_REALM_CHARACTERS_INIT: one zero-count row
      // per realm so the realm list shows the account.
      await connection.execute(
        `INSERT INTO realmcharacters (realmid, acctid, numchars)
         SELECT r.id, a.id, 0
         FROM realmlist r
         JOIN account a ON a.username = ?
         LEFT JOIN realmcharacters rc ON rc.realmid = r.id AND rc.acctid = a.id
         WHERE rc.acctid IS NULL`,
        [username],
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
   * Replaces an existing account's credentials. The caller must have verified
   * the account exists. Matching is case-insensitive via the column collation.
   */
  async resetPassword(username: string, password: string): Promise<void> {
    const { salt, verifier } = makeRegistrationData(username, password);
    await this.pool.execute("UPDATE account SET salt = ?, verifier = ? WHERE username = ?", [
      salt,
      verifier,
      username,
    ]);
  }
}
