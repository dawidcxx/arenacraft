import { NEXT_TOURNAMENT } from "./site";

export type TournamentStatus = "finished" | "upcoming";

export type Tournament = {
  id: string;
  number: number;
  status: TournamentStatus;
  title: string;
  description: string;
  date: string;
  format: string;
  startsAt: Date | null;
  prizes: Array<{ amount: string; description: string; highlight: boolean }>;
};

export const TOURNAMENTS: Tournament[] = [
  {
    id: "arenacraft-forever",
    number: 1,
    status: "finished",
    title: "Arenacraft forever",
    description:
      "A smaller tournament to keep the pace going. Due to concurrent WoW Forever release we expect a smaller player count. A more chill tourney, we will experiment with MCH format here. The goal here is continue tech development for larger tourneys",
    date: "18:00-21:00, Sunday 8th of November (CET timezone)",
    format: "SoloQ MCH, King of the Hill",
    startsAt: null,
    prizes: [
      { amount: "50 EUR", description: "to the highest rated player", highlight: true },
      {
        amount: "25 EUR",
        description:
          "lottery reward to a randomly selected player. The winner is excluded from the reward.",
        highlight: false,
      },
    ],
  },
  {
    id: "no-warmane-november",
    number: 2,
    status: "upcoming",
    title: "No Warmane november",
    description:
      "Time to get serious! Prize pool and details might change slightly depending on the player feedback. We wanna measure the impact of a slightly higher prize on the games quality. By this point we expect our tech to mature. Fewer bugs, more smooth game play experience.",
    date: "18:00-21:00, Sunday 15th of November (CET timezone)",
    format: "SoloQ Flex, King of the Hill",
    startsAt: NEXT_TOURNAMENT.startsAt,
    prizes: [
      { amount: "100 EUR", description: "to the highest rated player", highlight: true },
      {
        amount: "50 EUR",
        description:
          "lottery reward to a randomly selected player. The winner is excluded from the reward.",
        highlight: false,
      },
    ],
  },
];

export const TOURNAMENT_NOTE =
  "Depending on how those 2 go I will let u know the December schedule.";

export const TOURNAMENT_RULES = [
  "Players with a poor reputation are banned from participating. If you are curious if fall under this category DM me @Randomsheep 99,9% of players need not to worry about it",
  "Griefing is strictly prohibited. Expect harsh punishments",
  "We reserve the rights to adjust the rules at any moment",
  "Avoid toxic behavior towards lower skill players",
  "Joining Voice Chat, even if full mute is encouraged",
  "Lottery win chance scales with your play count, you need to complete 5 won games played to participate",
];