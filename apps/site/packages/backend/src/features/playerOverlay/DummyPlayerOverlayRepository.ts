import type { OverlayRecord } from "@arenacraft/common";
import type { OverlayProfileRow } from "./playerOverlay";
import type { PlayerOverlayRepository } from "./PlayerOverlayRepository";

/** Local development token: open `/overlay/dummyoverlaytoken000000000000000`. */
export const DUMMY_OVERLAY_TOKEN = "dummyoverlaytoken000000000000000";

const DUMMY_GUID = 1;

const DUMMY_PROFILE: OverlayProfileRow = {
  charName: "Mithrandir",
  classId: 8,
  accountId: 1,
  rating: 1700,
  seasonWins: 52,
  seasonGames: 73,
};

// Local development data. No database is required with DATA_SOURCE=dummy (the default).
export class DummyPlayerOverlayRepository implements PlayerOverlayRepository {
  async characterGuidByToken(token: string): Promise<number | null> {
    return token === DUMMY_OVERLAY_TOKEN ? DUMMY_GUID : null;
  }

  async profile(guid: number): Promise<OverlayProfileRow | null> {
    return guid === DUMMY_GUID ? DUMMY_PROFILE : null;
  }

  async alias(): Promise<string | null> {
    return null;
  }

  async todayRecord(): Promise<OverlayRecord> {
    return { wins: 2, losses: 0 };
  }

  async recentResults(_guid: number, limit: number): Promise<boolean[]> {
    return [true, true, false, false, true, false].slice(0, Math.max(0, Math.floor(limit)));
  }
}