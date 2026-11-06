import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction, Guild, GuildBan } from "discord.js";

import { hasVipRole } from "./aliasCommand";
import type { AccountBan, AccountRef, AuthDb } from "./authDb";
import { normalizeDiscordId } from "./discordId";
import { logger } from "./logger";

const UNKNOWN_BAN_CODE = 10026;

// Hidden by default like /alias and /charlist; VIP gets access via a
// per-command overwrite in Server Settings -> Integrations.
export const BANINFO_COMMAND = new SlashCommandBuilder()
  .setName("baninfo")
  .setDescription("Show core and Discord ban information for a user (VIP only)")
  .setDMPermission(false)
  .setDefaultMemberPermissions(0n)
  .addStringOption((option) =>
    option
      .setName("discord_acc")
      .setDescription("The Discord user ID or mention to inspect")
      .setRequired(true)
      // Room for a raw snowflake or a <@!...> mention.
      .setMaxLength(24),
  )
  .toJSON();

export interface DiscordBanStatus {
  banned: boolean | null;
  reason?: string | null;
}

/** Mirrors the core's active-ban check in BanMgr / the login queries. */
export function isBanActive(ban: AccountBan, now: number): boolean {
  if (!ban.active) return false;
  return ban.unbandate === ban.bandate || ban.unbandate > now;
}

export function currentBan(bans: AccountBan[], now: number): AccountBan | null {
  return bans.find((ban) => isBanActive(ban, now)) ?? null;
}

function formatCoreBan(ban: AccountBan): string {
  if (ban.unbandate === ban.bandate) {
    return `Permanent — since <t:${ban.bandate}:f>`;
  }
  return `Until <t:${ban.unbandate}:f> (<t:${ban.unbandate}:R>)`;
}

function formatDiscordBan(status: DiscordBanStatus): string {
  if (status.banned === null) return "**Discord server ban:** unknown (could not query Discord)";
  if (!status.banned) return "**Discord server ban:** no";
  const reason = status.reason ? ` — reason: ${status.reason}` : "";
  return `**Discord server ban:** yes${reason}`;
}

export function formatBanInfo(
  discordId: string,
  account: AccountRef | null,
  bans: AccountBan[],
  discordBan: DiscordBanStatus,
  now: number,
): string {
  const lines = [`**Discord:** <@${discordId}> (\`${discordId}\`)`];

  if (!account) {
    lines.push("**Core account:** none linked", "**Core ban:** n/a");
  } else {
    lines.push(`**Core account:** \`${account.username}\` (#${account.id})`);

    const ban = currentBan(bans, now);
    if (ban) {
      lines.push(`**Core ban:** ${formatCoreBan(ban)}`);
      lines.push(`**Banned by:** ${ban.bannedBy}`, `**Reason:** ${ban.reason}`);
    } else {
      lines.push("**Core ban:** none");
    }

    const otherBans = bans.length - (ban ? 1 : 0);
    if (otherBans > 0) lines.push(`**Other ban records:** ${otherBans}`);
  }

  lines.push(formatDiscordBan(discordBan));
  return lines.join("\n");
}

async function fetchDiscordBan(guild: Guild, discordId: string): Promise<DiscordBanStatus> {
  try {
    const ban: GuildBan = await guild.bans.fetch({ user: discordId, force: true });
    return { banned: true, reason: ban.reason ?? null };
  } catch (error) {
    if (error && typeof error === "object" && (error as { code?: number }).code === UNKNOWN_BAN_CODE) {
      return { banned: false };
    }
    throw error;
  }
}

export async function handleBanInfo(
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

  const discordId = normalizeDiscordId(interaction.options.getString("discord_acc", true));
  if (!discordId) {
    await interaction.reply({
      content: "Provide a valid Discord user ID or mention.",
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  const guild = interaction.guild;
  if (!guild) {
    await interaction.reply({ content: "This command can only be used in the server.", flags: MessageFlags.Ephemeral });
    return;
  }

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const discordBanPromise = fetchDiscordBan(guild, discordId).catch((error) => {
    logger.warn(`Could not fetch Discord ban for ${discordId}`, error);
    return { banned: null } satisfies DiscordBanStatus;
  });
  const [account, discordBan] = await Promise.all([
    authDb.accountByDiscordId(discordId),
    discordBanPromise,
  ]);
  const bans = account ? await authDb.accountBans(account.id) : [];

  const now = Math.floor(Date.now() / 1000);
  await interaction.editReply(formatBanInfo(discordId, account, bans, discordBan, now));
}
