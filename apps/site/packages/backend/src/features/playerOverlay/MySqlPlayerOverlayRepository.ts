import type { OverlayRecord } from "@arenacraft/common";
import { and, desc, eq, gte, inArray, sql, type SQL } from "drizzle-orm";
import type { RowDataPacket } from "mysql2/promise";
import { arenaTeam, arenaTeamMember, characters } from "@/features/ladder/model/ladderTables";
import { accountAlias } from "@/features/liveLadder/model/liveLadderTables";
import type { DrizzleDb } from "@/shared/Db";
import { accountOverlay, arenaReplay, arenaReplayPlayer } from "./model/playerOverlayTables";
import type { OverlayProfileRow } from "./playerOverlay";
import type { PlayerOverlayRepository } from "./PlayerOverlayRepository";

// The soloq team of every character is a 5v5 arena team it captains
// (src/game/Arenacraft/soloq/SoloqTeam.cpp).
const ARENA_TYPE_5V5 = 5;

// arenacraft::replay::Bracket::Soloq (src/game/Arenacraft/replay/ReplayConfig.hpp).
const REPLAY_BRACKET_SOLOQ = 4;

// `arena_replay.winner` is a TeamId; 2 (TEAM_NEUTRAL) is a draw or a cancelled start, which has no
// rating change and is left out.
const DECIDED_WINNERS = [0, 1];

type Query = { toSQL(): { sql: string; params: unknown[] } };

export class MySqlPlayerOverlayRepository implements PlayerOverlayRepository {
  constructor(
    private readonly characterDb: DrizzleDb,
    private readonly authDb: DrizzleDb | null,
    private readonly timeoutMs: number,
  ) {}

  async characterGuidByToken(token: string): Promise<number | null> {
    if (this.authDb === null) return null;

    const query = this.authDb
      .select({ guid: accountOverlay.guid })
      .from(accountOverlay)
      .where(eq(accountOverlay.token, token))
      .limit(1);

    const [row] = await this.rows(this.authDb, query);
    return row === undefined ? null : Number(row[0]);
  }

  async profile(guid: number): Promise<OverlayProfileRow | null> {
    const query = this.characterDb
      .select({
        charName: characters.name,
        classId: characters.class,
        accountId: characters.account,
        rating: arenaTeamMember.personalRating,
        seasonWins: arenaTeamMember.seasonWins,
        seasonGames: arenaTeamMember.seasonGames,
      })
      .from(characters)
      .leftJoin(
        arenaTeam,
        and(eq(arenaTeam.captainGuid, characters.guid), eq(arenaTeam.type, ARENA_TYPE_5V5)),
      )
      .leftJoin(
        arenaTeamMember,
        and(
          eq(arenaTeamMember.arenaTeamId, arenaTeam.arenaTeamId),
          eq(arenaTeamMember.guid, characters.guid),
        ),
      )
      .where(eq(characters.guid, guid))
      .limit(1);

    const [row] = await this.rows(this.characterDb, query);
    if (row === undefined) return null;

    const [charName, classId, accountId, rating, seasonWins, seasonGames] = row;
    return {
      charName: String(charName),
      classId: Number(classId),
      accountId: Number(accountId),
      rating: rating === null ? null : Number(rating),
      seasonWins: Number(seasonWins ?? 0),
      seasonGames: Number(seasonGames ?? 0),
    };
  }

  async alias(accountId: number): Promise<string | null> {
    if (this.authDb === null) return null;

    const query = this.authDb
      .select({ alias: accountAlias.alias })
      .from(accountAlias)
      .where(eq(accountAlias.accountId, accountId))
      .limit(1);

    const [row] = await this.rows(this.authDb, query);
    const alias = row === undefined ? "" : String(row[0]);
    return alias === "" ? null : alias;
  }

  async todayRecord(guid: number): Promise<OverlayRecord> {
    const won = sql`${arenaReplay.winner} = ${arenaReplayPlayer.team}`;
    const query = this.characterDb
      .select({
        wins: sql<number>`COALESCE(SUM(${won}), 0)`,
        losses: sql<number>`COALESCE(SUM(NOT (${won})), 0)`,
      })
      .from(arenaReplayPlayer)
      .innerJoin(arenaReplay, eq(arenaReplay.id, arenaReplayPlayer.replayId))
      // `created_at` is written with NOW() by the core, so CURDATE() is the same server clock.
      .where(soloqMatchesOf(guid, gte(arenaReplay.createdAt, sql`CURDATE()`)));

    const [row] = await this.rows(this.characterDb, query);
    return { wins: Number(row?.[0] ?? 0), losses: Number(row?.[1] ?? 0) };
  }

  async recentResults(guid: number, limit: number): Promise<boolean[]> {
    const safeLimit = Math.max(0, Math.floor(limit));
    const query = this.characterDb
      .select({ won: sql<number>`${arenaReplay.winner} = ${arenaReplayPlayer.team}` })
      .from(arenaReplayPlayer)
      .innerJoin(arenaReplay, eq(arenaReplay.id, arenaReplayPlayer.replayId))
      .where(soloqMatchesOf(guid))
      .orderBy(desc(arenaReplay.id))
      .limit(safeLimit);

    const rows = await this.rows(this.characterDb, query);
    return rows.map(([won]) => Number(won) === 1);
  }

  private async rows(db: DrizzleDb, query: Query): Promise<unknown[][]> {
    const { sql: text, params } = query.toSQL();
    const [rows] = await db.$client.query<RowDataPacket[]>({
      sql: text,
      values: params,
      timeout: this.timeoutMs,
      rowsAsArray: true,
    });
    return rows as unknown as unknown[][];
  }
}

function soloqMatchesOf(guid: number, extra?: SQL): SQL | undefined {
  return and(
    eq(arenaReplayPlayer.guid, guid),
    eq(arenaReplay.bracket, REPLAY_BRACKET_SOLOQ),
    inArray(arenaReplay.winner, DECIDED_WINNERS),
    extra,
  );
}