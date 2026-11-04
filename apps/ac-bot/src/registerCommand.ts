import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import { generatePassword } from "./credentials";
import { logger } from "./logger";
import { grantPlayer } from "./onboarding";
import { MAX_ACCOUNT_NAME_LENGTH, normalizeAccountName } from "./wowName";

export const REGISTER_COMMAND = new SlashCommandBuilder()
  .setName("register")
  .setDescription("Create your Arenacraft login or reset its password")
  .toJSON();

/** The login lines sent by /register. `realmlist` comes from REALMLIST; empty leaves its line out. */
export function credentialsBlock(username: string, password: string, realmlist: string): string {
  const lines = [`**Username**  => \`${username}\``, `**Password**  => \`${password}\``];
  // the line players put into Data/<locale>/realmlist.wtf of the 3.3.5 client
  if (realmlist) lines.push(`**Realmlist** => \`set realmlist ${realmlist}\``);
  return lines.join("\n");
}

export async function handleRegister(
  interaction: ChatInputCommandInteraction,
  authDb: AuthDb,
  realmlist: string,
): Promise<void> {
  const discordId = interaction.user.id;
  const discordUsername = interaction.user.username;
  const accountName = normalizeAccountName(discordUsername);

  // Acknowledge within Discord's 3s window; the DB/SRP work below can be slower.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const owned = await authDb.accountByDiscordId(discordId);
  if (owned) {
    const password = generatePassword();
    await authDb.resetPasswordForAccount(owned.id, password);
    await finish(interaction, owned.username, password, realmlist, "Password reset! Your new login:", discordUsername, "Reset");
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
  await finish(
    interaction,
    accountName,
    password,
    realmlist,
    "Welcome to Arenacraft! Your login:",
    discordUsername,
    "Registered",
  );
}

async function finish(
  interaction: ChatInputCommandInteraction,
  accountName: string,
  password: string,
  realmlist: string,
  greeting: string,
  discordUsername: string,
  action: "Registered" | "Reset",
): Promise<void> {
  // Having an account opens the rest of the server (its channels need the Player role).
  const opened = interaction.member instanceof GuildMember && (await grantPlayer(interaction.member));
  const footer =
    opened && action === "Registered"
      ? "\n\nThe server is open for you now - pick your class in the pinned message in #bot."
      : "";
  const reply = `${greeting}\n\n${credentialsBlock(accountName.toLowerCase(), password, realmlist)}${footer}`;
  await interaction.editReply(reply);

  try {
    await interaction.user.send(reply);
  } catch (error) {
    logger.warn(`Could not DM credentials to ${discordUsername}`, error);
  }

  logger.info(`${action} account ${accountName} (Discord user ${interaction.user.id}, ${discordUsername})`);
}
