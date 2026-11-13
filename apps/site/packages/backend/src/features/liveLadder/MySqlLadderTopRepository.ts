import { and, asc, desc, eq, gt, inArray } from "drizzle-orm";
import type { RowDataPacket } from "mysql2/promise";
import {
  arenaTeam,
  arenaTeamMember,
  characterSoloqRole,
  characters,
} from "@/features/ladder/model/ladderTables";
import type { DrizzleDb } from "@/shared/Db";
import type { LiveLadderRow } from "./liveLadder";
import { accountAlias } from "./model/liveLadderTables";

// The soloq team of every character is a 5v5 arena team it captains
// (src/game/Arenacraft/soloq/SoloqTeam.cpp).
const ARENA_TYPE_5V5 = 5;

export class MySqlLadderTopRepository {
  constructor(
    private readonly characterDb: DrizzleDb,
    private readonly authDb: DrizzleDb | null,
    private readonly timeoutMs: number,
  ) {}

  async topSoloq(limit: number): Promise<LiveLadderRow[]> {
    const safeLimit = Math.max(0, Math.floor(limit));

    const query = this.characterDb
      .select({
        charName: characters.name,
        classId: characters.class,
        accountId: characters.account,
        rating: arenaTeamMember.personalRating,
        seasonWins: arenaTeamMember.seasonWins,
        seasonGames: arenaTeamMember.seasonGames,
      })
      .from(arenaTeam)
      .innerJoin(characters, eq(characters.guid, arenaTeam.captainGuid))
      .innerJoin(
        arenaTeamMember,
        and(
          eq(arenaTeamMember.arenaTeamId, arenaTeam.arenaTeamId),
          eq(arenaTeamMember.guid, characters.guid),
        ),
      )
      .leftJoin(characterSoloqRole, eq(characterSoloqRole.guid, characters.guid))
      .where(and(eq(arenaTeam.type, ARENA_TYPE_5V5), gt(arenaTeamMember.seasonGames, 0)))
      .orderBy(desc(arenaTeamMember.personalRating), asc(characters.name))
      .limit(safeLimit)
      .toSQL();

    const [rows] = await this.characterDb.$client.query<RowDataPacket[]>({
      sql: query.sql,
      values: query.params,
      timeout: this.timeoutMs,
      rowsAsArray: true,
    });

    return (rows as unknown as unknown[][]).map(
      ([charName, classId, accountId, rating, seasonWins, seasonGames]) => ({
        charName: String(charName),
        classId: Number(classId),
        accountId: Number(accountId),
        rating: Number(rating),
        seasonWins: Number(seasonWins),
        seasonGames: Number(seasonGames),
      }),
    );
  }

  async findAliases(accountIds: number[]): Promise<Map<number, string>> {
    const aliases = new Map<number, string>();
    if (this.authDb === null || accountIds.length === 0) return aliases;

    const query = this.authDb
      .select({ accountId: accountAlias.accountId, alias: accountAlias.alias })
      .from(accountAlias)
      .where(inArray(accountAlias.accountId, accountIds))
      .toSQL();

    const [rows] = await this.authDb.$client.query<RowDataPacket[]>({
      sql: query.sql,
      values: query.params,
      timeout: this.timeoutMs,
      rowsAsArray: true,
    });

    for (const [accountId, alias] of rows as unknown as unknown[][]) {
      const value = String(alias);
      if (value !== "") aliases.set(Number(accountId), value);
    }
    return aliases;
  }
}