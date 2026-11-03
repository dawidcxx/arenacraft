import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { logger } from "./logger";

export const SET_PASSWORD_COMMAND = new SlashCommandBuilder()
  .setName("setpassword")
  .setDescription("Set the password for your Arenacraft login")
  .addStringOption((option) => option.setName("password").setDescription("Your new password").setRequired(true))
  .toJSON();

export async function handleSetPassword(interaction: ChatInputCommandInteraction, authDb: AuthDb): Promise<void> {
  const password = interaction.options.getString("password", true);

  // Acknowledge within Discord's 3s window; the DB writes below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const account = await authDb.accountByDiscordId(interaction.user.id);
  if (!account) {
    await interaction.editReply(
      "You don't have a linked Arenacraft account yet. Run `/register` first (use `old_password` if you already have a login).",
    );
    return;
  }

  await authDb.resetPasswordForAccount(account.id, password);
  await interaction.editReply(`Password updated for \`${account.username}\`. Use it the next time you log in.`);
  logger.info(`Set password for account ${account.username} (Discord user ${interaction.user.id})`);
}
