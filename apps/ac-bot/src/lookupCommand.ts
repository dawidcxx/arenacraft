import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import type { CharacterDb } from "./characterDb";

export const LOOKUP_COMMAND = new SlashCommandBuilder()
  .setName("lookup")
  .setDescription("Look up the Discord user behind a character name")
  .addStringOption((option) =>
    option.setName("character").setDescription("The character name to look up").setRequired(true),
  )
  .toJSON();

/** Finds the guild member whose Discord username matches the account name. */
export function findMemberByUsername<T extends { user: { username: string } }>(
  members: Iterable<T>,
  username: string,
): T | undefined {
  const target = username.toLowerCase();
  for (const member of members) if (member.user.username.toLowerCase() === target) return member;
  return undefined;
}

export function formatLookup(charName: string, accountName: string, discordId: string | null): string {
  if (discordId === null) {
    return `**${charName}** is on account \`${accountName}\`, but no matching Discord member was found.`;
  }
  return `**${charName}** is played by <@${discordId}> (\`${discordId}\`).`;
}

export interface LookupDeps {
  authDb: AuthDb;
  characterDb: CharacterDb;
}

export async function handleLookup(interaction: ChatInputCommandInteraction, deps: LookupDeps): Promise<void> {
  const charName = interaction.options.getString("character", true).trim();
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const accountId = await deps.characterDb.accountIdByCharacterName(charName);
  if (accountId === null) {
    await interaction.editReply(`Character **${charName}** not found.`);
    return;
  }

  const accountName = (await deps.authDb.accountNames([accountId])).get(accountId);
  if (!accountName) {
    await interaction.editReply(`Character **${charName}** is not attached to an account.`);
    return;
  }

  const guild = interaction.guild;
  let member = guild ? findMemberByUsername(guild.members.cache.values(), accountName) : undefined;
  // Startup already populates the cache; only refetch when it is empty (e.g. a
  // cold cache) instead of hitting the API on every unknown lookup.
  if (!member && guild && guild.members.cache.size === 0) {
    await guild.members.fetch();
    member = findMemberByUsername(guild.members.cache.values(), accountName);
  }

  await interaction.editReply(formatLookup(charName, accountName, member?.id ?? null));
}
