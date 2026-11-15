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
};

export const HALL_OF_FAME: readonly Champion[] = [
  {
    id: "arenacraft-forever",
    winner: "Kontek",
    wowClass: "Hunter",
    spec: null,
    tournament: "Arenacraft forever",
    date: "Sunday, 8 November 2026",
    format: "SoloQ MCH, King of the Hill",
    prize: "50 EUR",
  },
  {
    id: "flex-soloq-2026-11-01",
    winner: "Bruce",
    wowClass: "Paladin",
    spec: "Holy",
    tournament: "Flex SoloQ tourney",
    date: "Sunday, 1 November 2026",
    format: "SoloQ Flex",
    prize: "100 EUR",
    twitch: "bruce_tv1",
    note: "Bruce donated the whole 100 EUR prize back to the server to fund upcoming events. Huge gratz, and thank you!",
  },
];
