import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create your ArenaCraft login")
  .toJSON();

function credentialsBlock(username: string, password: string): string {
  return ["```", username, password, "```"].join("\n");
}

export async function handleRegister(interaction: ChatInputCommandInteraction, authDb: AuthDb): Promise<void> {
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);

  // Acknowledge within Discord's 3s window; the DB/SRP work below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  if (await authDb.accountExists(accountName)) {
    await interaction.editReply(
      `An account named \`${accountName}\` is already registered. If you lost your password, ask an admin.`,
    );
    return;
  }

  const password = generatePassword();
  await authDb.createAccount(accountName, password);

  const reply = `Welcome to ArenaCraft! Your login:\n${credentialsBlock(accountName, password)}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`Registered account ${accountName} for Discord user ${discordUsername}`);
}
