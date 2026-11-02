import mysql from "mysql2/promise";
import type { Pool, RowDataPacket } from "mysql2/promise";

import type { LadderRow } from "./ladder";

// The soloq team of every character is a 5v5 arena team it captains (src/game/Arenacraft/soloq/SoloqTeam.cpp).
const ARENA_TYPE_5V5 = 5;

export class LadderDb {
  private readonly pool: Pool;

  constructor(
    url: string,
    private readonly timeoutMs: number,
  ) {
    // Tiny pool: the cache in front of it means at most one query per TTL.
    this.pool = mysql.createPool({ uri: url, connectionLimit: 2, connectTimeout: timeoutMs, enableKeepAlive: true });
  }

  async close(): Promise<void> {
    await this.pool.end();
  }

  /** Characters that played at least one soloq game this season, best personal rating first. */
  async topSoloq(limit: number): Promise<LadderRow[]> {
    const safeLimit = Math.max(0, Math.floor(limit));
    const [rows] = await this.pool.query<RowDataPacket[]>({
      sql: `SELECT c.name AS charName, c.class AS classId, r.role AS storedRole,
                   m.personalRating AS rating, m.seasonWins AS seasonWins, m.seasonGames AS seasonGames
            FROM arena_team t
            JOIN characters c ON c.guid = t.captainGuid
            JOIN arena_team_member m ON m.arenaTeamId = t.arenaTeamId AND m.guid = c.guid
            LEFT JOIN character_soloq_role r ON r.guid = c.guid
            WHERE t.type = ? AND m.seasonGames > 0
            ORDER BY m.personalRating DESC, c.name ASC
            LIMIT ${safeLimit}`,
      values: [ARENA_TYPE_5V5],
      timeout: this.timeoutMs,
    });

    return rows.map((row) => ({
      charName: String(row.charName),
      classId: Number(row.classId),
      storedRole: row.storedRole === null || row.storedRole === undefined ? null : Number(row.storedRole),
      rating: Number(row.rating),
      seasonWins: Number(row.seasonWins),
      seasonGames: Number(row.seasonGames),
    }));
  }
}
