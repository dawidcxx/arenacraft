import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { MAX_ACCOUNT_NAME_LENGTH, normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create or link your Arenacraft login")
  .addStringOption((option) =>
    option
      .setName("old_password")
      .setDescription("Only when linking an existing account: your current in-game password")
      .setRequired(false),
  )
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

  const linked = await authDb.accountByDiscordId(discordId);
  if (linked) {
    const password = generatePassword();
    await authDb.resetPasswordForAccount(linked.id, password);
    await finish(interaction, linked.username, password, "Password reset! Your new login:", discordUsername);
    return;
  }

  const password = generatePassword();
  const existingId = await authDb.accountIdByName(accountName);

  if (existingId !== null) {
    const existingDiscordId = await authDb.discordIdByAccountId(existingId);
    if (existingDiscordId !== null) {
      await interaction.editReply(
        `The account name \`${accountName}\` is already linked to another Discord user. Contact an admin.`,
      );
      return;
    }

    const oldPassword = interaction.options.getString("old_password");
    if (!oldPassword) {
      await interaction.editReply(
        `An Arenacraft account named \`${accountName}\` already exists but is not linked to Discord yet.\n` +
          `Run \`/register\` again with \`old_password\` set to your current in-game password to link it.`,
      );
      return;
    }

    if (!(await authDb.verifyAccountPassword(existingId, oldPassword))) {
      await interaction.editReply("That password is incorrect. Linking cancelled.");
      return;
    }

    await authDb.bindDiscordId(existingId, discordId);
    await authDb.resetPasswordForAccount(existingId, password);
    await finish(interaction, accountName, password, "Account linked! Your new login:", discordUsername);
    return;
  }

  if (accountName.length === 0 || accountName.length > MAX_ACCOUNT_NAME_LENGTH) {
    await interaction.editReply(
      `Your Discord username cannot be used as a WoW login name (must be 1-${MAX_ACCOUNT_NAME_LENGTH} characters). Contact an admin.`,
    );
    return;
  }

  await authDb.createAccount(discordId, accountName, password);
  await finish(interaction, accountName, password, "Welcome to Arenacraft! Your login:", discordUsername);
}

async function finish(
  interaction: ChatInputCommandInteraction,
  accountName: string,
  password: string,
  greeting: string,
  discordUsername: string,
): Promise<void> {
  const reply = `${greeting}\n\n${credentialsBlock(accountName, password)}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`Updated credentials for account ${accountName} (Discord user ${interaction.user.id}, ${discordUsername})`);
}
