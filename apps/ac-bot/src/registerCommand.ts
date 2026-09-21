import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create your Arenacraft login")
  .toJSON();

function credentialsBlock(username: string, password: string): string {
  return [`**Username**  => \`${username}\``, `**Password**  => \`${password}\``].join("\n");
}

export async function handleRegister(interaction: ChatInputCommandInteraction, authDb: AuthDb): Promise<void> {
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);

  // Acknowledge within Discord's 3s window; the DB/SRP work below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const registered = await authDb.accountExists(accountName);
  const password = generatePassword();
  if (registered) await authDb.resetPassword(accountName, password);
  else await authDb.createAccount(accountName, password);

  const greeting = registered ? "Password reset! Your new login:" : "Welcome to Arenacraft! Your login:";
  const reply = `${greeting}\n\n${credentialsBlock(accountName, password)}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`${registered ? "Reset" : "Registered"} account ${accountName} for Discord user ${discordUsername}`);
}
