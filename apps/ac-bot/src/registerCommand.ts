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

  if (!accountName) {
    await interaction.reply({
      content:
        `Your Discord name \`${discordUsername}\` can't be used as an ArenaCraft account name.\n` +
        "Supported names are 2-20 letters or digits (no dots, dashes, underscores or spaces).",
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  if (await authDb.accountExists(accountName)) {
    await interaction.reply({
      content: `An account named \`${accountName}\` is already registered. If you lost your password, ask an admin.`,
      flags: MessageFlags.Ephemeral,
    });
    return;
  }

  const password = generatePassword();
  await authDb.createAccount(accountName, password);

  const reply = `Welcome to ArenaCraft! Your login:\n${credentialsBlock(accountName, password)}`;
  await interaction.reply({ content: reply, flags: MessageFlags.Ephemeral });

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`Registered account ${accountName} for Discord user ${discordUsername}`);
}
