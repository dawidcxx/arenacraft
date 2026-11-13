import { describe, expect, test } from "bun:test";
import type { OverlayRecord } from "@arenacraft/common";
import { DUMMY_OVERLAY_TOKEN, DummyPlayerOverlayRepository } from "./DummyPlayerOverlayRepository";
import { PlayerOverlayNotFoundError } from "./error/playerOverlayErrors";
import { toPlayerOverlay, type OverlayProfileRow } from "./playerOverlay";
import type { PlayerOverlayRepository } from "./PlayerOverlayRepository";
import { PlayerOverlayService } from "./PlayerOverlayService";

const PROFILE: OverlayProfileRow = {
  charName: "Strugly",
  classId: 8,
  accountId: 7,
  rating: 1700,
  seasonWins: 52,
  seasonGames: 73,
};

describe("toPlayerOverlay", () => {
  test("maps season stats, today and the last five matches oldest first", () => {
    const overlay = toPlayerOverlay(
      PROFILE,
      null,
      { wins: 2, losses: 0 },
      [true, true, false, false, true, false],
      123,
    );

    expect(overlay).toEqual({
      name: "Strugly",
      class: "Mage",
      rating: 1700,
      all: { wins: 52, losses: 21 },
      today: { wins: 2, losses: 0 },
      lastMatches: ["W", "L", "L", "W", "W"],
      updatedAt: 123,
    });
  });

  test("prefers the account alias, like the live ladder", () => {
    expect(toPlayerOverlay(PROFILE, "Venruki", { wins: 0, losses: 0 }, [], 0).name).toBe("Venruki");
  });

  test("keeps a missing SoloQ team as a null rating and an empty record", () => {
    const overlay = toPlayerOverlay(
      { ...PROFILE, rating: null, seasonWins: 0, seasonGames: 0 },
      null,
      { wins: 0, losses: 0 },
      [],
      0,
    );
    expect(overlay.rating).toBeNull();
    expect(overlay.all).toEqual({ wins: 0, losses: 0 });
    expect(overlay.lastMatches).toEqual([]);
  });
});

class CountingRepository implements PlayerOverlayRepository {
  calls = 0;

  async characterGuidByToken(token: string): Promise<number | null> {
    this.calls++;
    return token === "known" ? 1 : null;
  }

  async profile(): Promise<OverlayProfileRow | null> {
    return PROFILE;
  }

  async alias(): Promise<string | null> {
    return null;
  }

  async todayRecord(): Promise<OverlayRecord> {
    return { wins: 1, losses: 1 };
  }

  async recentResults(): Promise<boolean[]> {
    return [true, false];
  }
}

describe("PlayerOverlayService", () => {
  test("serves repeated requests for one token from the snapshot cache", async () => {
    const repository = new CountingRepository();
    const service = new PlayerOverlayService(repository, () => 1_000);

    await service.get("known");
    await service.get("known");

    expect(repository.calls).toBe(1);
  });

  test("rejects unknown tokens with a not-found error", async () => {
    const service = new PlayerOverlayService(new CountingRepository());
    await expect(service.get("missing")).rejects.toBeInstanceOf(PlayerOverlayNotFoundError);
  });

  test("the dummy repository answers the documented development token", async () => {
    const service = new PlayerOverlayService(new DummyPlayerOverlayRepository());
    const overlay = await service.get(DUMMY_OVERLAY_TOKEN);
    expect(overlay.lastMatches).toEqual(["W", "L", "L", "W", "W"]);
    expect(overlay.today).toEqual({ wins: 2, losses: 0 });
  });
});