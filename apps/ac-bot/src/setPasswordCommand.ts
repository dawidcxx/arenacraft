import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { logger } from "./logger";
import { normalizeAccountName } from "./wowName";

export const SET_PASSWORD_COMMAND = new SlashCommandBuilder()
  .setName("setpassword")
  .setDescription("Set the password for your Arenacraft login")
  .addStringOption((option) => option.setName("password").setDescription("Your new password").setRequired(true))
  .toJSON();

export async function handleSetPassword(interaction: ChatInputCommandInteraction, authDb: AuthDb): Promise<void> {
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);
  const password = interaction.options.getString("password", true);

  // Acknowledge within Discord's 3s window; the DB writes below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  if (!(await authDb.accountExists(accountName))) {
    await interaction.editReply("You don't have an Arenacraft account yet. Run /register first.");
    return;
  }

  await authDb.resetPassword(accountName, password);
  await interaction.editReply("Password updated. Use it the next time you log in.");
  logger.info(`Set password for account ${accountName} (Discord user ${discordUsername})`);
}
