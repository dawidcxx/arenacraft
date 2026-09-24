import mysql from "mysql2/promise";
import type { Pool, RowDataPacket } from "mysql2/promise";

// ARENA_TYPE_5v5; soloq reuses the otherwise-unused 5v5 track for its per-player
// team, so every soloq team is a type-5 arena_team.
const ARENA_TYPE_5V5 = 5;

export interface SoloqRanker {
  accountId: number;
  charName: string;
  classId: number;
  rating: number;
  seasonWins: number;
  seasonGames: number;
}

export class CharacterDb {
  private readonly pool: Pool;

  constructor(url: string) {
    this.pool = mysql.createPool(url);
  }

  async close(): Promise<void> {
    await this.pool.end();
  }

  /** Top soloq players by personal rating across the whole ladder. */
  async topSoloqRankers(limit: number): Promise<SoloqRanker[]> {
    const safeLimit = Math.max(0, Math.floor(limit));
    const [rows] = await this.pool.execute<RowDataPacket[]>(
      `SELECT c.account AS accountId, c.name AS charName, c.class AS classId,
              m.personalRating AS rating, m.seasonWins AS seasonWins, m.seasonGames AS seasonGames
       FROM arena_team t
       JOIN characters c ON c.guid = t.captainGuid
       JOIN arena_team_member m ON m.arenaTeamId = t.arenaTeamId AND m.guid = c.guid
       WHERE t.type = ?
       ORDER BY m.personalRating DESC, c.name ASC
       LIMIT ${safeLimit}`,
      [ARENA_TYPE_5V5],
    );

    return rows.map((row) => ({
      accountId: Number(row.accountId),
      charName: String(row.charName),
      classId: Number(row.classId),
      rating: Number(row.rating),
      seasonWins: Number(row.seasonWins),
      seasonGames: Number(row.seasonGames),
    }));
  }
}
