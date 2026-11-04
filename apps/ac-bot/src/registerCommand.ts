import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { MAX_ACCOUNT_NAME_LENGTH, normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create your Arenacraft login or reset its password")
  .toJSON();

function credentialsBlock(username: string, password: string): string {
  return [`**Username**  => \`${username}\``, `**Password**  => \`${password}\``].join("\n");
}

export async function handleRegister(interaction: ChatInputCommandInteraction, authDb: AuthDb): Promise<void> {
  const discordId = interaction.user.id;
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);

  // Acknowledge within Discord's 3s window; the DB/SRP work below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const owned = await authDb.accountByDiscordId(discordId);
  if (owned) {
    const password = generatePassword();
    await authDb.resetPasswordForAccount(owned.id, password);
    await finish(interaction, owned.username, password, "Password reset! Your new login:", discordUsername, "Reset");
    return;
  }

  if (accountName.length === 0 || accountName.length > MAX_ACCOUNT_NAME_LENGTH) {
    await interaction.editReply(
      `Your Discord username cannot be used as a WoW login name (must be 1-${MAX_ACCOUNT_NAME_LENGTH} characters). Contact an admin.`,
    );
    return;
  }

  const password = generatePassword();
  await authDb.createAccount(discordId, accountName, password);
  await finish(interaction, accountName, password, "Welcome to Arenacraft! Your login:", discordUsername, "Registered");
}

async function finish(
  interaction: ChatInputCommandInteraction,
  accountName: string,
  password: string,
  greeting: string,
  discordUsername: string,
  action: "Registered" | "Reset",
): Promise<void> {
  const reply = `${greeting}\n\n${credentialsBlock(accountName.toLowerCase(), password)}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`${action} account ${accountName} (Discord user ${interaction.user.id}, ${discordUsername})`);
}
