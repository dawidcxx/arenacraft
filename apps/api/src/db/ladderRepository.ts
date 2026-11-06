import { and, asc, desc, eq, gt } from "drizzle-orm";
import type { RowDataPacket } from "mysql2/promise";

import type { LadderRow } from "../ladder/ladder";
import type { Database } from "./client";
import { arenaTeam, arenaTeamMember, characters, characterSoloqRole } from "./schema";

// The soloq team of every character is a 5v5 arena team it captains
// (src/game/Arenacraft/soloq/SoloqTeam.cpp).
const ARENA_TYPE_5V5 = 5;

export class LadderRepository {
  constructor(
    private readonly db: Database,
    private readonly timeoutMs: number,
  ) {}

  /** Characters that played at least one soloq game this season, best personal rating first. */
  async topSoloq(limit: number): Promise<LadderRow[]> {
    const safeLimit = Math.max(0, Math.floor(limit));

    const query = this.db
      .select({
        charName: characters.name,
        classId: characters.class,
        storedRole: characterSoloqRole.role,
        rating: arenaTeamMember.personalRating,
        seasonWins: arenaTeamMember.seasonWins,
        seasonGames: arenaTeamMember.seasonGames,
      })
      .from(arenaTeam)
      .innerJoin(characters, eq(characters.guid, arenaTeam.captainGuid))
      .innerJoin(
        arenaTeamMember,
        and(eq(arenaTeamMember.arenaTeamId, arenaTeam.arenaTeamId), eq(arenaTeamMember.guid, characters.guid)),
      )
      .leftJoin(characterSoloqRole, eq(characterSoloqRole.guid, characters.guid))
      .where(and(eq(arenaTeam.type, ARENA_TYPE_5V5), gt(arenaTeamMember.seasonGames, 0)))
      .orderBy(desc(arenaTeamMember.personalRating), asc(characters.name))
      .limit(safeLimit)
      .toSQL();

    // Execute through the underlying pool rather than Drizzle's prepared query so the per-query
    // timeout (DB_TIMEOUT_MS) is still enforced by mysql2. Drizzle's SQL has no column aliases
    // (`characters`.`name`, `arena_team_member`.`personalRating`, ...), so rows keyed by name would
    // not carry the select keys above (charName, rating, ...). Read them as arrays, in select order,
    // the way Drizzle itself maps results.
    const [rows] = await this.db.$client.query<RowDataPacket[]>({
      sql: query.sql,
      values: query.params,
      timeout: this.timeoutMs,
      rowsAsArray: true,
    });

    return (rows as unknown as unknown[][]).map(
      ([charName, classId, storedRole, rating, seasonWins, seasonGames]) => ({
        charName: String(charName),
        classId: Number(classId),
        storedRole: storedRole === null || storedRole === undefined ? null : Number(storedRole),
        rating: Number(rating),
        seasonWins: Number(seasonWins),
        seasonGames: Number(seasonGames),
      }),
    );
  }
}
