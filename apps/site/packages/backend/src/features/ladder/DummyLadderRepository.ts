import type { LadderRow } from "./ladder";
import type { LadderRepository } from "./LadderRepository";

// Local development data. No database is required with DATA_SOURCE=dummy (the default).
const DUMMY_ROWS: LadderRow[] = [
  {
    charName: "Kalvax",
    classId: 4,
    storedRole: null,
    rating: 2412,
    seasonWins: 184,
    seasonGames: 245,
  },
  {
    charName: "Mithrandir",
    classId: 8,
    storedRole: null,
    rating: 2368,
    seasonWins: 171,
    seasonGames: 232,
  },
  {
    charName: "Grimtide",
    classId: 9,
    storedRole: null,
    rating: 2325,
    seasonWins: 166,
    seasonGames: 229,
  },
  {
    charName: "Durotan",
    classId: 1,
    storedRole: null,
    rating: 2289,
    seasonWins: 158,
    seasonGames: 224,
  },
  {
    charName: "Frostmourne",
    classId: 6,
    storedRole: null,
    rating: 2244,
    seasonWins: 149,
    seasonGames: 218,
  },
  {
    charName: "Anduin",
    classId: 5,
    storedRole: 2,
    rating: 2210,
    seasonWins: 142,
    seasonGames: 211,
  },
  {
    charName: "Sylvanas",
    classId: 3,
    storedRole: null,
    rating: 2187,
    seasonWins: 138,
    seasonGames: 209,
  },
  { charName: "Uther", classId: 2, storedRole: 2, rating: 2150, seasonWins: 131, seasonGames: 205 },
  {
    charName: "Thrall",
    classId: 7,
    storedRole: 1,
    rating: 2118,
    seasonWins: 126,
    seasonGames: 202,
  },
  {
    charName: "Malfurion",
    classId: 11,
    storedRole: 1,
    rating: 2086,
    seasonWins: 120,
    seasonGames: 198,
  },
  {
    charName: "Jaina",
    classId: 8,
    storedRole: null,
    rating: 2044,
    seasonWins: 113,
    seasonGames: 193,
  },
  {
    charName: "Rexxar",
    classId: 3,
    storedRole: 0,
    rating: 2011,
    seasonWins: 107,
    seasonGames: 190,
  },
];

export class DummyLadderRepository implements LadderRepository {
  async topSoloq(limit: number): Promise<LadderRow[]> {
    return DUMMY_ROWS.slice(0, Math.max(0, Math.floor(limit)));
  }
}