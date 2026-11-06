import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import { hasVipRole } from "./aliasCommand";
import type { AuthDb } from "./authDb";
import { normalizeDiscordId } from "./discordId";
import { logger } from "./logger";

// "0" hides the command from everyone except admins and members granted a
// per-command overwrite. Discord only permits OAuth2 bearer tokens to edit
// those overwrites (a bot token is rejected), so the VIP role has to be
// allowed once in Server Settings -> Integrations -> Manage -> /permaban.
// Overwrites survive restarts because bulk registration upserts by name.
export const PERMABAN_COMMAND = new SlashCommandBuilder()
  .setName("permaban")
  .setDescription("Permanently ban a Discord user and their Arenacraft account (VIP only)")
  .setDMPermission(false)
  .setDefaultMemberPermissions(0n)
  .addStringOption((option) =>
    option
      .setName("discord_id")
      .setDescription("The Discord user ID to ban")
      .setRequired(true)
      // Room for a raw snowflake or a <@!...> mention.
      .setMaxLength(24),
  )
  .addStringOption((option) =>
    option.setName("reason").setDescription("Reason for the permanent ban").setRequired(true).setMaxLength(255),
  )
  .toJSON();

export function formatPermaBanReply(discordId: string, coreSummary: string): string {
  return `Permanently banned <@${discordId}> from Discord. Core: ${coreSummary}.`;
}

export async function handlePermaBan(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  vipRoleName: string,
): Promise<void> {
  const member = interaction.member;
  // Guild-only command, but keep the runtime check in case permissions are
  // misconfigured: Discord visibility is not an authorization boundary.
  if (!(member instanceof GuildMember) || !hasVipRole(member.roles.cache.values(), vipRoleName)) {
    await interaction.reply({ content: "You need the VIP role to use this command.", flags: MessageFlags.Ephemeral });
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

  const reason = interaction.options.getString("reason", true).trim();
  if (reason.length === 0) {
    await interaction.reply({ content: "A ban reason is required.", flags: MessageFlags.Ephemeral });
    return;
  }

  const guild = interaction.guild;
  if (!guild) {
    await interaction.reply({ content: "This command can only be used in the server.", flags: MessageFlags.Ephemeral });
    return;
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  // Ban on Discord first so a hierarchy/permission failure cannot leave the
  // linked game account banned without the Discord ban the caller asked for.
  try {
    await guild.bans.create(discordId, { reason });
  } catch (error) {
    logger.error(`Discord ban failed for ${discordId}`, error);
    await interaction.editReply(
      "The Discord ban failed; the core account was not touched. Check the bot's Ban Members permission and role hierarchy.",
    );
    return;
  }

  let coreSummary = "no linked core account";
  try {
    const account = await authDb.accountByDiscordId(discordId);
    if (account) {
      await authDb.banAccount(account.id, reason, `Discord (${interaction.user.id})`);
      coreSummary = `core account \`${account.username}\` (#${account.id})`;
    }
  } catch (error) {
    logger.error(`Core ban failed for ${discordId}`, error);
    await interaction.editReply(`Banned <@${discordId}> from Discord, but the core ban failed.`);
    return;
  }

  logger.info(
    `Permanently banned Discord user ${discordId} (${coreSummary}) by ${interaction.user.id}: ${reason}`,
  );
  await interaction.editReply(formatPermaBanReply(discordId, coreSummary));
}
