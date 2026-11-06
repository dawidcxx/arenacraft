import { randomUUID } from "node:crypto";

import {
  GuildMember,
  LabelBuilder,
  MessageFlags,
  ModalBuilder,
  SlashCommandBuilder,
  TextDisplayBuilder,
  TextInputBuilder,
  TextInputStyle,
} from "discord.js";
import type { ChatInputCommandInteraction, ModalSubmitInteraction } from "discord.js";

import { hasVipRole } from "./aliasCommand";
import type { AuthDb } from "./authDb";
import { normalizeDiscordId } from "./discordId";
import { logger } from "./logger";

export const PERMABAN_MODAL_PREFIX = "permaban-confirm";
const PERMABAN_CONFIRMATION = "BAN";
const PENDING_TTL_MS = 5 * 60_000;

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

interface PendingPermaBan {
  userId: string;
  discordId: string;
  reason: string;
  expiresAt: number;
}

// The target/reason travel with the modal submit through this short-lived map;
// the custom id itself only carries the nonce.
const pendingPermaBans = new Map<string, PendingPermaBan>();

export function isPermaBanConfirmed(value: string): boolean {
  return value.trim().toUpperCase() === PERMABAN_CONFIRMATION;
}

export function formatPermaBanReply(discordId: string, coreSummary: string): string {
  return `Permanently banned <@${discordId}> from Discord. Core: ${coreSummary}.`;
}

function storePendingPermaBan(userId: string, discordId: string, reason: string): string {
  const now = Date.now();
  for (const [nonce, pending] of pendingPermaBans) {
    if (pending.expiresAt <= now) pendingPermaBans.delete(nonce);
  }

  const nonce = randomUUID();
  pendingPermaBans.set(nonce, { userId, discordId, reason, expiresAt: now + PENDING_TTL_MS });
  return `${PERMABAN_MODAL_PREFIX}:${nonce}`;
}

interface PendingPermaBanRef {
  nonce: string;
  pending: PendingPermaBan;
}

function findPendingPermaBan(customId: string, userId: string): PendingPermaBanRef | null {
  const prefix = `${PERMABAN_MODAL_PREFIX}:`;
  if (!customId.startsWith(prefix)) return null;

  const nonce = customId.slice(prefix.length);
  const pending = pendingPermaBans.get(nonce);
  if (!pending) return null;
  if (pending.expiresAt <= Date.now()) {
    pendingPermaBans.delete(nonce);
    return null;
  }
  if (pending.userId !== userId) return null;

  return { nonce, pending };
}

export function buildPermaBanModal(customId: string, discordId: string, reason: string): ModalBuilder {
  return new ModalBuilder()
    .setCustomId(customId)
    .setTitle("Confirm permanent ban")
    .addTextDisplayComponents(
      new TextDisplayBuilder().setContent(
        `**Target:** <@${discordId}> (\`${discordId}\`)\n**Reason:** ${reason}`,
      ),
    )
    .addLabelComponents(
      new LabelBuilder()
        .setLabel(`Type ${PERMABAN_CONFIRMATION} to confirm`)
        .setDescription("This permanently bans the Discord user and their linked core account.")
        .setTextInputComponent(
          new TextInputBuilder()
            .setCustomId("confirm")
            .setStyle(TextInputStyle.Short)
            .setMinLength(PERMABAN_CONFIRMATION.length)
            .setMaxLength(PERMABAN_CONFIRMATION.length)
            .setRequired(true),
        ),
    );
}

export async function handlePermaBan(
  interaction: ChatInputCommandInteraction,
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

  const customId = storePendingPermaBan(interaction.user.id, discordId, reason);
  await interaction.showModal(buildPermaBanModal(customId, discordId, reason));
}

export async function handlePermaBanConfirmation(
  interaction: ModalSubmitInteraction,
  authDb: AuthDb,
  vipRoleName: string,
): Promise<void> {
  const member = interaction.member;
  if (!(member instanceof GuildMember) || !hasVipRole(member.roles.cache.values(), vipRoleName)) {
    await interaction.reply({ content: "You need the VIP role to use this command.", flags: MessageFlags.Ephemeral });
    return;
  }

  const pendingRef = findPendingPermaBan(interaction.customId, interaction.user.id);
  if (!pendingRef) {
    await interaction.reply({
      content: "This permaban confirmation has expired or is invalid. Run `/permaban` again.",
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  if (!isPermaBanConfirmed(interaction.fields.getTextInputValue("confirm"))) {
    await interaction.reply({
      content: `Type \`${PERMABAN_CONFIRMATION}\` exactly to confirm the permanent ban. Nothing was changed; run \`/permaban\` again to retry.`,
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  pendingPermaBans.delete(pendingRef.nonce);
  await executePermaBan(interaction, authDb, pendingRef.pending.discordId, pendingRef.pending.reason);
}

async function executePermaBan(
  interaction: ChatInputCommandInteraction | ModalSubmitInteraction,
  authDb: AuthDb,
  discordId: string,
  reason: string,
): Promise<void> {
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const guild = interaction.guild;
  if (!guild) {
    await interaction.editReply("This command can only be used in the server.");
    return;
  }

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
