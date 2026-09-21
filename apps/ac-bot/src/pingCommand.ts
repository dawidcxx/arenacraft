import { SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

export const PING_COMMAND = new SlashCommandBuilder()
  .setName("ping")
  .setDescription("Say hi to the Arenacraft bot")
  .toJSON();

export function pingGreeting(mention: string): string {
  return `Hello ${mention}! Welcome to Arenacraft.`;
}

export async function handlePing(interaction: ChatInputCommandInteraction): Promise<void> {
  // Public reply (not ephemeral) so everyone in the channel sees the greeting.
  await interaction.reply(pingGreeting(interaction.user.toString()));
}
