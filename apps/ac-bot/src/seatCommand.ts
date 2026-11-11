import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import { hasVipRole } from "./aliasCommand";
import type { AuthDb, SeatPage, SeatRow, SeatTier, StoredSeatTier } from "./authDb";
import { normalizeDiscordId } from "./discordId";
import { logger } from "./logger";

const SEAT_TIERS: readonly SeatTier[] = ["None", "Gold", "Diamond"];

export const MY_SEAT_COMMAND = new SlashCommandBuilder()
  .setName("myseat")
  .setDescription("Show your tournament seating tier")
  .setDMPermission(false)
  .toJSON();

// Same permission model as the other VIP commands: visible to admins by
// default, with the VIP role granted a per-command overwrite in Discord.
export const SET_SEAT_COMMAND = new SlashCommandBuilder()
  .setName("setseat")
  .setDescription("Set a player's tournament seating tier (VIP only)")
  .setDMPermission(false)
  .setDefaultMemberPermissions(0n)
  .addStringOption((option) =>
    option
      .setName("discord_id")
      .setDescription("The Discord user ID or mention to seat")
      .setRequired(true)
      .setMaxLength(24),
  )
  .addStringOption((option) =>
    option
      .setName("tier")
      .setDescription("The seating tier")
      .setRequired(true)
      .addChoices(
        { name: "None", value: "None" },
        { name: "Gold", value: "Gold" },
        { name: "Diamond", value: "Diamond" },
      ),
  )
  .toJSON();

export const GET_SEAT_LIST_COMMAND = new SlashCommandBuilder()
  .setName("getseatlist")
  .setDescription("List players in the Gold or Diamond tournament seats")
  .setDMPermission(false)
  .addStringOption((option) =>
    option
      .setName("tier")
      .setDescription("Seat tier to list")
      .setRequired(true)
      .addChoices({ name: "Gold", value: "Gold" }, { name: "Diamond", value: "Diamond" }),
  )
  .toJSON();

/** Canonicalises a tier name, returning null for anything else. */
export function parseSeatTier(value: string): SeatTier | null {
  const wanted = value.trim().toLowerCase();
  return SEAT_TIERS.find((tier) => tier.toLowerCase() === wanted) ?? null;
}

export function formatMySeat(tier: SeatTier): string {
  return `Your seat: **${tier}**.`;
}

const HEADERS = ["#", "Account", "Seated"] as const;

/** `/getseatlist` only shows this many of the newest seats per tier. */
export const SEAT_LIST_LIMIT = 20;

function formatSeatDate(date: Date): string {
  if (Number.isNaN(date.getTime())) return "unknown";
  return date.toISOString().slice(0, 10);
}

export function formatSeatList(tier: StoredSeatTier, rows: SeatRow[], total: number = rows.length): string {
  if (total === 0) return `No players have the ${tier} seat.`;

  const sorted = [...rows].sort(
    (left, right) =>
      right.updatedAt.getTime() - left.updatedAt.getTime() || left.username.localeCompare(right.username),
  );
  const cells = sorted.map((row, index) => ({
    "#": String(index + 1),
    Account: row.username,
    Seated: formatSeatDate(row.updatedAt),
  }));

  const widths = HEADERS.map((header) => Math.max(header.length, ...cells.map((row) => row[header].length)));
  const render = (values: string[]) => values.map((value, i) => value.padEnd(widths[i]!)).join("  ").trimEnd();

  const heading =
    total > sorted.length
      ? `${tier} seats (showing the ${sorted.length} most recent of ${total})`
      : `${tier} seats (${total})`;
  const lines = [render([...HEADERS]), ...cells.map((row) => render(HEADERS.map((header) => row[header])))];
  return [heading, "```", ...lines, "```"].join("\n");
}

export async function handleMySeat(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  loadSeats: () => Promise<SeatRow[]>,
): Promise<void> {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const account = await authDb.accountByDiscordId(interaction.user.id);
  let tier: SeatTier = "None";
  if (account) {
    const seat = (await loadSeats()).find((row) => row.accountId === account.id);
    if (seat) tier = seat.tier;
  }

  await interaction.editReply(formatMySeat(tier));
}

export async function handleSetSeat(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  vipRoleName: string,
  onChanged: () => void,
): Promise<void> {
  const member = interaction.member;
  if (!(member instanceof GuildMember) || !hasVipRole(member.roles.cache.values(), vipRoleName)) {
    await interaction.reply({ content: "You need the VIP role to set seats.", flags: MessageFlags.Ephemeral });
    return;
  }

  const discordId = normalizeDiscordId(interaction.options.getString("discord_id", true));
  if (!discordId) {
    await interaction.reply({
      content: "Provide a valid Discord user ID or mention.",
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  const tier = parseSeatTier(interaction.options.getString("tier", true));
  if (!tier) {
    await interaction.reply({ content: "Unknown seat tier.", flags: MessageFlags.Ephemeral });
    return;
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const account = await authDb.accountByDiscordId(discordId);
  if (!account) {
    await interaction.editReply(`<@${discordId}> has no linked Arenacraft account yet.`);
    return;
  }

  await authDb.setSeat(account.id, tier);
  onChanged();

  logger.info(`Set seat for account ${account.username} to ${tier} (by Discord ${interaction.user.id})`);
  await interaction.editReply(
    tier === "None"
      ? `Cleared <@${discordId}>'s seat (None).`
      : `Set <@${discordId}>'s seat to **${tier}**.`,
  );
}

export async function handleGetSeatList(
  interaction: ChatInputCommandInteraction,
  loadSeatPage: (tier: StoredSeatTier) => Promise<SeatPage>,
): Promise<void> {
  const tier = parseSeatTier(interaction.options.getString("tier", true));
  if (tier !== "Gold" && tier !== "Diamond") {
    await interaction.reply({ content: "Choose Gold or Diamond.", flags: MessageFlags.Ephemeral });
    return;
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const { rows, total } = await loadSeatPage(tier);
  await interaction.editReply(formatSeatList(tier, rows, total));
}
