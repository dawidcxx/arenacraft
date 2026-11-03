import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { grantPlayer } from "./onboarding";
import { normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create your Arenacraft login")
  .toJSON();

export function credentialsBlock(username: string, password: string, realmlist: string): string {
  const lines = [`**Username**  => \`${username}\``, `**Password**  => \`${password}\``];
  // Data/<locale>/realmlist.wtf of the 3.3.5 client
  if (realmlist) lines.push(`**Realmlist** => \`set realmlist ${realmlist}\``);
  return lines.join("\n");
}

export async function handleRegister(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  realmlist: string,
): Promise<void> {
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);

  // Acknowledge within Discord's 3s window; the DB/SRP work below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const registered = await authDb.accountExists(accountName);
  const password = generatePassword();
  if (registered) await authDb.resetPassword(accountName, password);
  else await authDb.createAccount(accountName, password);

  // Registering opens the rest of the server (channels need the Player role).
  const opened = interaction.member instanceof GuildMember && (await grantPlayer(interaction.member));

  const greeting = registered ? "Password reset! Your new login:" : "Welcome to Arenacraft! Your login:";
  const footer = opened && !registered ? "\n\nThe server is open for you now - pick your class in the pinned message in #bot." : "";
  const reply = `${greeting}\n\n${credentialsBlock(accountName, password, realmlist)}${footer}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`${registered ? "Reset" : "Registered"} account ${accountName} for Discord user ${discordUsername}`);
}
