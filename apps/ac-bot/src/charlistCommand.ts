import { GuildMember, MessageFlags, SlashCommandBuilder } from "discord.js";
import type { ChatInputCommandInteraction, Guild } from "discord.js";

import { hasVipRole } from "./aliasCommand";
import type { AuthDb } from "./authDb";
import type { CharacterDb, CharacterSummary } from "./characterDb";
import { classDisplayName } from "./topRankersCommand";

// "0" hides the command from everyone except admins and members granted a
// per-command overwrite. Discord only permits OAuth2 bearer tokens to edit
// those overwrites (a bot token is rejected), so the VIP role has to be
// allowed once in Server Settings -> Integrations -> Manage -> /charlist.
export const CHAR_LIST_COMMAND = new SlashCommandBuilder()
  .setName("charlist")
  .setDescription("List the characters owned by a Discord member (VIP only)")
  .setDMPermission(false)
  .setDefaultMemberPermissions(0n)
  .addStringOption((option) =>
    option
      .setName("discord")
      .setDescription("Discord username, display name, mention, or user ID")
      .setRequired(true),
  )
  .toJSON();

export interface MemberLike {
  id: string;
  user: { username: string; globalName?: string | null };
  displayName?: string;
}

const MENTION_PATTERN = /^<@!?(\d+)>$/;
const SNOWFLAKE_PATTERN = /^\d+$/;

export interface MemberQuery {
  id: string | null;
  name: string | null;
}

/** Accepts a mention, a raw user ID, or a username/display name (with an optional leading @). */
export function parseMemberQuery(query: string): MemberQuery {
  const trimmed = query.trim();

  const mention = trimmed.match(MENTION_PATTERN);
  if (mention) return { id: mention[1] ?? null, name: null };
  if (SNOWFLAKE_PATTERN.test(trimmed)) return { id: trimmed, name: null };

  const name = trimmed.replace(/^@/, "").toLowerCase();
  return { id: null, name: name.length > 0 ? name : null };
}

export function findMemberByName<T extends MemberLike>(members: Iterable<T>, query: string): T | undefined {
  const parsed = parseMemberQuery(query);
  if (parsed.id === null && parsed.name === null) return undefined;

  const candidates = [...members];
  if (parsed.id !== null) return candidates.find((member) => member.id === parsed.id);

  // Prefer the immutable username over display names, which can collide.
  const byUsername = candidates.find((member) => member.user.username.toLowerCase() === parsed.name);
  if (byUsername) return byUsername;

  return candidates.find(
    (member) =>
      member.user.globalName?.toLowerCase() === parsed.name || member.displayName?.toLowerCase() === parsed.name,
  );
}

export function formatCharList(displayName: string, chars: CharacterSummary[]): string {
  if (chars.length === 0) return `**${displayName}** has no characters.`;
  return chars.map((char) => `- ${char.charName}, ${classDisplayName(char.classId)}`).join("\n");
}

export interface CharListDeps {
  authDb: AuthDb;
  characterDb: CharacterDb;
}

async function resolveMember(guild: Guild | null, query: string): Promise<GuildMember | undefined> {
  if (!guild) return undefined;

  let member = findMemberByName(guild.members.cache.values(), query);
  // Startup already populates the cache; only refetch when it is empty (e.g. a
  // cold cache) instead of hitting the API on every unknown lookup.
  if (!member && guild.members.cache.size === 0) {
    await guild.members.fetch();
    member = findMemberByName(guild.members.cache.values(), query);
  }
  return member;
}

export async function handleCharList(
  interaction: ChatInputCommandInteraction,
  deps: CharListDeps,
  vipRoleName: string,
): Promise<void> {
  const requester = interaction.member;
  // Guild-only command, but keep the runtime check in case permissions are
  // misconfigured: Discord visibility is not an authorization boundary.
  if (!(requester instanceof GuildMember) || !hasVipRole(requester.roles.cache.values(), vipRoleName)) {
    await interaction.reply({ content: "You need the VIP role to use this command.", flags: MessageFlags.Ephemeral });
    return;
  }

  const query = interaction.options.getString("discord", true).trim();
  await interaction.deferReply({ flags: MessageFlags.Ephemeral });

  const member = await resolveMember(interaction.guild, query);
  if (!member) {
    await interaction.editReply(`No Discord member found for \`${query}\`.`);
    return;
  }

  const accountIds = await deps.authDb.accountIdsByDiscordId(member.id);
  if (accountIds.length === 0) {
    await interaction.editReply(`<@${member.id}> has no Arenacraft account yet.`);
    return;
  }

  const chars = await deps.characterDb.charactersByAccountIds(accountIds);
  await interaction.editReply(formatCharList(member.displayName || member.user.username, chars));
}
