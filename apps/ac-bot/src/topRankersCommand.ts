import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import type { CharacterDb } from "./characterDb";

export const TOP_RANKERS_LIMIT = 10;

export const TOP_RANKERS_COMMAND = new SlashCommandBuilder()
  .setName("toprankers")
  .setDescription(`Show the top ${TOP_RANKERS_LIMIT} solo queue players`)
  .toJSON();

// SharedDefines.h Classes enum.
const CLASS_NAMES: Record<number, string> = {
  1: "Warrior",
  2: "Paladin",
  3: "Hunter",
  4: "Rogue",
  5: "Priest",
  6: "Death Knight",
  7: "Shaman",
  8: "Mage",
  9: "Warlock",
  11: "Druid",
};

export function classDisplayName(classId: number): string {
  return CLASS_NAMES[classId] ?? "Unknown";
}

export interface Ranker {
  discordName: string;
  charName: string;
  classId: number;
  rating: number;
  seasonWins: number;
  seasonGames: number;
}

export interface RankerDeps {
  authDb: AuthDb;
  characterDb: CharacterDb;
}

export async function loadTopRankers(deps: RankerDeps, limit: number = TOP_RANKERS_LIMIT): Promise<Ranker[]> {
  const rows = await deps.characterDb.topSoloqRankers(limit);
  const names = await deps.authDb.accountNames(rows.map((row) => row.accountId));

  return rows.map((row) => ({
    discordName: names.get(row.accountId) ?? "unknown",
    charName: row.charName,
    classId: row.classId,
    rating: row.rating,
    seasonWins: row.seasonWins,
    seasonGames: row.seasonGames,
  }));
}

const HEADERS = ["#", "Discord name", "Char Name", "Class", "Rating", "Stats"] as const;

function statLine(ranker: Ranker): string {
  const losses = Math.max(0, ranker.seasonGames - ranker.seasonWins);
  return `${ranker.seasonWins}-${losses}`;
}

export function formatTopRankers(rankers: Ranker[]): string {
  if (rankers.length === 0) return "No solo queue players are ranked yet.";

  const cells = rankers.map((ranker, index) => ({
    "#": String(index + 1),
    "Discord name": ranker.discordName,
    "Char Name": ranker.charName,
    Class: classDisplayName(ranker.classId),
    Rating: String(ranker.rating),
    Stats: statLine(ranker),
  }));

  const widths = HEADERS.map((header) => Math.max(header.length, ...cells.map((row) => row[header].length)));
  const render = (values: string[]) => values.map((value, i) => value.padEnd(widths[i]!)).join("  ").trimEnd();

  const lines = [render([...HEADERS]), ...cells.map((row) => render(HEADERS.map((header) => row[header])))];
  return [`SoloQ Top ${TOP_RANKERS_LIMIT}`, "```", ...lines, "```"].join("\n");
}

export async function handleTopRankers(
  interaction: ChatInputCommandInteraction,
  loadRankers: () => Promise<Ranker[]>,
): Promise<void> {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const rankers = await loadRankers();
  await interaction.editReply(formatTopRankers(rankers));
}
