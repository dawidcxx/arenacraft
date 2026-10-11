import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { CharacterDb, OnlineCharacter } from "./characterDb";
import { classDisplayName } from "./topRankersCommand";

export const ONLINE_LIST_COMMAND = new SlashCommandBuilder()
  .setName("onlinelist")
  .setDescription("Show the characters currently online")
  .toJSON();

export async function loadOnlineList(characterDb: CharacterDb): Promise<OnlineCharacter[]> {
  return characterDb.onlineCharacters();
}

export function formatOnlineList(chars: OnlineCharacter[]): string {
  if (chars.length === 0) return "No players are online right now.";

  const lines = chars.map((c) => `${c.charName} (${classDisplayName(c.classId)})`);
  return [`Online now (${chars.length})`, "```", ...lines, "```"].join("\n");
}

export async function handleOnlineList(
  interaction: ChatInputCommandInteraction,
  loadOnline: () => Promise<OnlineCharacter[]>,
): Promise<void> {
  // Ephemeral: the list is only useful to the caller and keeps the channel clean.
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });
  const chars = await loadOnline();
  await interaction.editReply(formatOnlineList(chars));
}
