import type { LiveLadderPlayer } from "@arenacraft/common";
import { injectable } from "@needle-di/core";
import { Logger } from "@/lib/Logger";
import {
  LIVE_LADDER_FULL_SYNC_INTERVAL_MS,
  LIVE_LADDER_POLL_INTERVAL_MS,
  LIVE_LADDER_SIZE,
  toLiveLadderPlayer,
  type LiveLadderRow,
} from "./liveLadder";
import { PollingLadderTopService } from "./PollingLadderTopService";

const logger = Logger.for("MockLadderTopService");

// A simulated game lands somewhere in this window, so the widget visibly moves every few seconds
// without being predictable. Events rotate through forced rank swaps, top-5 knockouts and regular
// games so every transition is easy to observe.
const MOCK_EVENT_MIN_MS = 5_000;
const MOCK_EVENT_MAX_MS = 20_000;

// Rotate the event kinds so a rank swap and a top-5 knockout both show up within the first two
// events instead of leaving it to chance.
const MOCK_EVENT_ROTATION = ["swap", "newcomer", "swap", "game", "newcomer", "game"] as const;
type MockEventKind = (typeof MOCK_EVENT_ROTATION)[number];

// Fixture ladder for DATA_SOURCE=dummy.
const MOCK_ROWS: LiveLadderRow[] = [
  { charName: "Kalvax", classId: 4, accountId: 1, rating: 2412, seasonWins: 184, seasonGames: 245 },
  {
    charName: "Mithrandir",
    classId: 8,
    accountId: 2,
    rating: 2368,
    seasonWins: 171,
    seasonGames: 232,
  },
  {
    charName: "Grimtide",
    classId: 9,
    accountId: 3,
    rating: 2325,
    seasonWins: 166,
    seasonGames: 229,
  },
  {
    charName: "Durotan",
    classId: 1,
    accountId: 4,
    rating: 2289,
    seasonWins: 158,
    seasonGames: 224,
  },
  {
    charName: "Frostmourne",
    classId: 6,
    accountId: 5,
    rating: 2244,
    seasonWins: 149,
    seasonGames: 218,
  },
  { charName: "Anduin", classId: 5, accountId: 6, rating: 2210, seasonWins: 142, seasonGames: 211 },
  {
    charName: "Sylvanas",
    classId: 3,
    accountId: 7,
    rating: 2187,
    seasonWins: 138,
    seasonGames: 209,
  },
];

// Only some accounts have a Discord alias, mirroring production.
const MOCK_ALIASES = new Map<number, string>([
  [2, "Mith"],
  [3, "Grim"],
  [6, "Anduin"],
]);

@injectable()
export class MockLadderTopService extends PollingLadderTopService {
  private readonly rows = MOCK_ROWS.map((row) => ({ ...row }));
  private nextEventAt = Date.now() + randomEventDelay();
  private eventIndex = 0;

  constructor() {
    super({
      pollIntervalMs: LIVE_LADDER_POLL_INTERVAL_MS,
      fullSyncIntervalMs: LIVE_LADDER_FULL_SYNC_INTERVAL_MS,
    });
  }

  protected async loadPlayers(): Promise<LiveLadderPlayer[]> {
    const now = Date.now();
    if (now >= this.nextEventAt) this.simulateGame(now);

    return this.rows
      .map((row) => ({ row, alias: MOCK_ALIASES.get(row.accountId) }))
      .sort((a, b) => b.row.rating - a.row.rating || a.row.charName.localeCompare(b.row.charName))
      .slice(0, LIVE_LADDER_SIZE)
      .map(({ row, alias }, index) => toLiveLadderPlayer(row, index + 1, alias));
  }

  private simulateGame(now: number) {
    this.nextEventAt = now + randomEventDelay();

    const ranked = [...this.rows].sort(compareRows);
    const top = ranked.slice(0, LIVE_LADDER_SIZE);
    const kind = MOCK_EVENT_ROTATION[this.eventIndex % MOCK_EVENT_ROTATION.length] ?? "game";
    this.eventIndex++;

    if (kind === "newcomer" && ranked.length > LIVE_LADDER_SIZE) {
      // Rank 6 beats rank 5 by enough to enter the visible list.
      const winner = ranked[LIVE_LADDER_SIZE];
      const loser = top[top.length - 1];
      if (winner === undefined || loser === undefined) return;

      const delta = crossingDelta(winner, loser);
      applyResult(winner, loser, delta);
      logEvent(kind, winner, loser, delta, this.nextEventAt - now);
      return;
    }

    if (kind === "swap" && top.length >= 2) {
      // The player directly below beats the one above, forcing a rank change.
      const winnerIndex = 1 + Math.floor(Math.random() * (top.length - 1));
      const winner = top[winnerIndex];
      const loser = top[winnerIndex - 1];
      if (winner === undefined || loser === undefined) return;

      const delta = crossingDelta(winner, loser);
      applyResult(winner, loser, delta);
      logEvent(kind, winner, loser, delta, this.nextEventAt - now);
      return;
    }

    // Regular game between two top-5 players: ratings move, a rank change is possible but not forced.
    if (top.length < 2) return;
    const winnerIndex = Math.floor(Math.random() * top.length);
    const winner = top[winnerIndex];
    const loser = top[pickOtherIndex(top.length, winnerIndex)];
    if (winner === undefined || loser === undefined) return;

    const delta = 8 + Math.floor(Math.random() * 17); // 8-24 rating
    applyResult(winner, loser, delta);
    logEvent(kind, winner, loser, delta, this.nextEventAt - now);
  }
}

function compareRows(a: LiveLadderRow, b: LiveLadderRow): number {
  return b.rating - a.rating || a.charName.localeCompare(b.charName);
}

/** Enough rating for the lower player to pass the higher one (both move by `delta`). */
function crossingDelta(winner: LiveLadderRow, loser: LiveLadderRow): number {
  const gap = Math.max(0, loser.rating - winner.rating);
  return Math.floor(gap / 2) + 4 + Math.floor(Math.random() * 6);
}

function applyResult(winner: LiveLadderRow, loser: LiveLadderRow, delta: number) {
  winner.rating += delta;
  winner.seasonWins += 1;
  winner.seasonGames += 1;
  loser.rating = Math.max(0, loser.rating - delta);
  loser.seasonGames += 1;
}

function logEvent(
  kind: MockEventKind,
  winner: LiveLadderRow,
  loser: LiveLadderRow,
  delta: number,
  nextInMs: number,
) {
  logger.debug("Mock ladder event", {
    kind,
    winner: winner.charName,
    loser: loser.charName,
    delta,
    nextInMs,
  });
}

function randomEventDelay(): number {
  return MOCK_EVENT_MIN_MS + Math.floor(Math.random() * (MOCK_EVENT_MAX_MS - MOCK_EVENT_MIN_MS));
}

function pickOtherIndex(length: number, excluded: number): number {
  const index = Math.floor(Math.random() * (length - 1));
  return index >= excluded ? index + 1 : index;
}