import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { logger } from "./logger";
import { normalizeAccountName } from "./wowName";

// Identifier-safe so addons can parse the in-game `ALIAS:<char>=<value>` line.
const ALIAS_PATTERN = /^[\p{L}\p{N}_-]{1,32}$/u;

// "0" hides the command from everyone except admins and members granted a
// per-command overwrite. Discord only permits OAuth2 bearer tokens to edit
// those overwrites (a bot token is rejected), so the VIP role has to be
// allowed once in Server Settings -> Integrations -> Manage -> /alias.
// Overwrites survive restarts because bulk registration upserts by name.
export const ALIAS_COMMAND = new SlashCommandBuilder()
  .setName("alias")
  .setDescription("Manage solo queue display aliases (VIP only)")
  .setDMPermission(false)
  .setDefaultMemberPermissions(0n)
  .addSubcommand((subcommand) =>
    subcommand
      .setName("set")
      .setDescription("Set or replace a player's alias")
      .addUserOption((option) =>
        option.setName("discord_id").setDescription("The Discord user to alias").setRequired(true),
      )
      .addStringOption((option) =>
        option
          .setName("alias")
          .setDescription("Alias shown in game (1-32 letters, digits, _ or -)")
          .setRequired(true),
      ),
  )
  .addSubcommand((subcommand) =>
    subcommand
      .setName("reset")
      .setDescription("Remove a player's alias")
      .addUserOption((option) =>
        option.setName("discord_id").setDescription("The Discord user to reset").setRequired(true),
      ),
  )
  .toJSON();

/** Returns the trimmed alias when it is a valid identifier, otherwise null. */
export function normalizeAlias(value: string): string | null {
  const trimmed = value.trim();
  if (!ALIAS_PATTERN.test(trimmed)) return null;
  return trimmed;
}

/** True when the role list contains the configured VIP role (case-insensitive). */
export function hasVipRole(roles: Iterable<{ name: string }>, roleName: string): boolean {
  const wanted = roleName.toLowerCase();
  for (const role of roles) if (role.name.toLowerCase() === wanted) return true;
  return false;
}

export async function handleAlias(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  vipRoleName: string,
): Promise<void> {
  const member = interaction.member;
  // Guild-only command, but keep the runtime check in case permissions are
  // misconfigured: Discord visibility is not an authorization boundary.
  if (!(member instanceof GuildMember) || !hasVipRole(member.roles.cache.values(), vipRoleName)) {
    await interaction.reply({ content: "You need the VIP role to manage aliases.", flags: MessageFlags.Ephemeral });
    return;
  }

  const target = interaction.options.getUser("discord_id", true);
  const accountName = normalizeAccountName(target.username);
  const subcommand = interaction.options.getSubcommand();

  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const accountId = await authDb.accountIdByName(accountName);
  if (accountId === null) {
    await interaction.editReply(`**${target.username}** has not registered an account yet.`);
    return;
  }

  if (subcommand === "set") {
    const alias = normalizeAlias(interaction.options.getString("alias", true));
    if (!alias) {
      await interaction.editReply("Alias must be 1-32 letters, digits, `_` or `-`.");
      return;
    }

    await authDb.setAlias(accountId, alias);
    await interaction.editReply(`Alias for **${target.username}** set to \`${alias}\`.`);
    logger.info(`Set alias ${alias} for account ${accountName}`);
    return;
  }

  if (subcommand === "reset") {
    const removed = await authDb.resetAlias(accountId);
    await interaction.editReply(
      removed ? `Alias for **${target.username}** removed.` : `**${target.username}** has no alias.`,
    );
    logger.info(`Reset alias for account ${accountName} (removed=${removed})`);
    return;
  }

  await interaction.editReply("Unknown subcommand.");
}
