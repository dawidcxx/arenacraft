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

interface Column {
  header: string;
  align: "left" | "right";
  value: (ranker: Ranker, index: number) => string;
}

function statLine(ranker: Ranker): string {
  const losses = Math.max(0, ranker.seasonGames - ranker.seasonWins);
  return `${ranker.seasonWins}-${losses}`;
}

// Discord renders no Markdown tables, so the table is drawn with box-drawing
// characters inside a code block and wrapped in an embed.
const COLUMNS: Column[] = [
  { header: "#", align: "right", value: (_ranker, index) => String(index + 1) },
  { header: "Discord", align: "left", value: (ranker) => ranker.discordName },
  { header: "Character", align: "left", value: (ranker) => ranker.charName },
  { header: "Class", align: "left", value: (ranker) => classDisplayName(ranker.classId) },
  { header: "Rating", align: "right", value: (ranker) => String(ranker.rating) },
  { header: "Stats", align: "left", value: (ranker) => statLine(ranker) },
];

/** Box-drawing table for the leaderboard, or an empty string when there are no rankers. */
export function formatTopRankers(rankers: Ranker[]): string {
  if (rankers.length === 0) return "";

  const cells = rankers.map((ranker, index) => COLUMNS.map((column) => column.value(ranker, index)));
  const widths = COLUMNS.map((column, i) => Math.max(column.header.length, ...cells.map((row) => row[i]!.length)));

  const border = (left: string, mid: string, right: string) =>
    left + widths.map((width) => "─".repeat(width + 2)).join(mid) + right;
  const renderRow = (values: string[]) =>
    "│" +
    values
      .map((value, i) => {
        const padded = COLUMNS[i]!.align === "right" ? value.padStart(widths[i]!) : value.padEnd(widths[i]!);
        return ` ${padded} `;
      })
      .join("│") +
    "│";

  const lines = [border("┌", "┬", "┐"), renderRow(COLUMNS.map((column) => column.header)), border("├", "┼", "┤")];
  for (const row of cells) lines.push(renderRow(row));
  lines.push(border("└", "┴", "┘"));

  return lines.join("\n");
}

const EMBED_COLOR = 0x5865f2;

export async function handleTopRankers(
  interaction: ChatInputCommandInteraction,
  loadRankers: () => Promise<Ranker[]>,
): Promise<void> {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const rankers = await loadRankers();
  const table = formatTopRankers(rankers);

  await interaction.editReply({
    embeds: [
      {
        title: `SoloQ Top ${TOP_RANKERS_LIMIT}`,
        description: table ? "```\n" + table + "\n```" : "No solo queue players are ranked yet.",
        color: EMBED_COLOR,
      },
    ],
  });
}
