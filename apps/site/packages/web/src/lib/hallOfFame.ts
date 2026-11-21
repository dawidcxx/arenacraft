/** Second place in a tournament. Usually no reward, so `prize` stays unset. */
export type RunnerUp = {
  /** Character name as shown on the realm. */
  winner: string;
  wowClass: string;
  spec: string | null;
  /** Placement prize, when there was one. */
  prize?: string;
};

/** A past tournament and the player who won it. Newest first. */
export type Champion = {
  id: string;
  /** Character name as shown on the realm. */
  winner: string;
  wowClass: string;
  spec: string | null;
  tournament: string;
  date: string;
  format: string;
  prize: string;
  /** Twitch login, when the champion streams. */
  twitch?: string;
  /** Shout-out shown under the card. */
  note?: string;
  /** Second place, if it was recorded. */
  runnerUp?: RunnerUp;
};

export const HALL_OF_FAME: readonly Champion[] = [
  {
    id: "no-warmane-november",
    winner: "Whiskas",
    wowClass: "Priest",
    spec: "Discipline",
    tournament: "No Warmane november",
    date: "Sunday, 15 November 2026",
    format: "SoloQ Flex, King of the Hill",
    prize: "100 EUR",
    runnerUp: {
      winner: "Addexio",
      wowClass: "Shaman",
      spec: "Restoration",
    },
  },
  {
    id: "arenacraft-forever",
    winner: "Kontek",
    wowClass: "Hunter",
    spec: "Marksman",
    tournament: "Arenacraft forever",
    date: "Sunday, 8 November 2026",
    format: "SoloQ MCH, King of the Hill",
    prize: "50 EUR",
    runnerUp: {
      winner: "Alexjacquot",
      wowClass: "Priest",
      spec: "Discipline",
    },
  },
  {
    id: "flex-soloq-2026-11-01",
    winner: "Bruce",
    wowClass: "Paladin",
    spec: "Holy",
    tournament: "Unannounced Brooxz tourney",
    date: "Sunday, 1 November 2026",
    format: "SoloQ Flex",
    prize: "100 EUR",
    twitch: "bruce_tv1",
    note: "Bruce donated the whole 100 EUR prize back to the server to fund upcoming events. Huge gratz, and thank you!",
    runnerUp: {
      winner: "Vrutel",
      wowClass: "Paladin",
      spec: "Holy",
    },
  },
];
