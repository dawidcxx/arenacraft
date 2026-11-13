import { randomBytes } from "node:crypto";

import { MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction } from "discord.js";

import type { AuthDb } from "./authDb";
import type { CharacterDb } from "./characterDb";
import { logger } from "./logger";

export const OVERLAY_COMMAND = new SlashCommandBuilder()
  .setName("overlay")
  .setDescription("Get your personal OBS overlay link with your SoloQ stats")
  .setDMPermission(false)
  .addStringOption((option) =>
    option
      .setName("character")
      .setDescription("The character the overlay shows")
      .setRequired(true)
      .setMaxLength(12),
  )
  .addBooleanOption((option) =>
    option.setName("new_link").setDescription("Issue a new link and disable the old one"),
  )
  .toJSON();

/** 24 random bytes as base64url: 32 url-safe characters, the format the website accepts. */
export function makeOverlayToken(): string {
  return randomBytes(24).toString("base64url");
}

export function overlayUrl(baseUrl: string, token: string): string {
  return `${baseUrl.replace(/\/+$/, "")}/${token}`;
}

export function formatOverlayReply(charName: string, url: string, renewed: boolean): string {
  return [
    renewed
      ? `New SoloQ overlay link for **${charName}**; the old link no longer works:`
      : `Your SoloQ overlay for **${charName}**:`,
    url,
    "Add it in OBS as a **Browser** source, 400 x 160. The background is transparent.",
    "Keep the link private. `/overlay` with `new_link: True` disables it and issues a new one.",
  ].join("\n");
}

export interface OverlayDeps {
  authDb: AuthDb;
  characterDb: CharacterDb;
  overlayBaseUrl: string;
}

export async function handleOverlay(interaction: ChatInputCommandInteraction, deps: OverlayDeps): Promise<void> {
  const charName = interaction.options.getString("character", true).trim();
  const newLink = interaction.options.getBoolean("new_link") ?? false;
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const accountIds = await deps.authDb.accountIdsByDiscordId(interaction.user.id);
  if (accountIds.length === 0) {
    await interaction.editReply("You have no Arenacraft account yet. Run `/register` first.");
    return;
  }

  const character = await deps.characterDb.characterByName(charName);
  if (!character || !accountIds.includes(character.accountId)) {
    await interaction.editReply(`**${charName}** is not one of your characters.`);
    return;
  }

  // One link per account: switching the character keeps the link, so the OBS source keeps working.
  const existing = await deps.authDb.overlayToken(character.accountId);
  const renewed = newLink && existing !== null;
  const token = existing === null || newLink ? makeOverlayToken() : existing;
  await deps.authDb.setOverlay(character.accountId, character.guid, token);

  logger.info(
    `Overlay for ${character.charName} (account ${character.accountId}) ${renewed ? "reissued" : "set"} by Discord ${interaction.user.id}`,
  );
  await interaction.editReply(
    formatOverlayReply(character.charName, overlayUrl(deps.overlayBaseUrl, token), renewed),
  );
}
