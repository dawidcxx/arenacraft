// Brings the Discord server in line with src/serverLayout.ts.
//
//   bun run setup:discord                    dry run: prints what would change, changes nothing
//   bun run setup:discord --apply            creates roles/categories/channels, moves channels, sets permissions
//   bun run setup:discord --sync-players     (with --apply) gives the Player role to members who already own a
//                                            game account (account_discord), so nobody who registered loses access
//
// Safe to run again: it only adds what is missing. Channels are never deleted or renamed (message history stays);
// existing permission overwrites are merged, not replaced. Reads the bot's .env (DISCORD_TOKEN, DISCORD_GUILD_ID,
// AUTH_DB_URL or AC_LOGIN_DATABASE_INFO for --sync-players).
import { ChannelType, Client, Events, GatewayIntentBits, PermissionFlagsBits } from "discord.js";
import type { CategoryChannel, ForumChannel, Guild, GuildBasedChannel, GuildChannel, Role } from "discord.js";

import { botOverwrite, EVERYONE, overwritesFor } from "../src/access";
import type { ChannelPermission } from "../src/access";
import { AuthDb } from "../src/authDb";
import { authDbUrlFromEnv } from "../src/config";
import { LAYOUT, ROLE_PLAYER, ROLES } from "../src/serverLayout";
import type { ChannelKind, ChannelSpec } from "../src/serverLayout";
import { describePlan, planSetup } from "../src/setupPlan";
import type { GuildSnapshot, SnapshotChannel } from "../src/setupPlan";

const apply = process.argv.includes("--apply");
const syncPlayers = process.argv.includes("--sync-players");

const token = process.env.DISCORD_TOKEN;
const guildId = process.env.DISCORD_GUILD_ID;
if (!token || !guildId) throw new Error("DISCORD_TOKEN and DISCORD_GUILD_ID must be set (apps/ac-bot/.env)");

function kindOf(channel: GuildBasedChannel): SnapshotChannel["kind"] {
  switch (channel.type) {
    case ChannelType.GuildCategory:
      return "category";
    case ChannelType.GuildText:
    case ChannelType.GuildAnnouncement:
      return "text";
    case ChannelType.GuildVoice:
      return "voice";
    case ChannelType.GuildForum:
      return "forum";
    default:
      return "other";
  }
}

function snapshot(guild: Guild): GuildSnapshot {
  const channels: SnapshotChannel[] = [];
  for (const channel of guild.channels.cache.values()) {
    if (channel.isThread()) continue;
    channels.push({
      id: channel.id,
      name: channel.name,
      kind: kindOf(channel),
      parentId: channel.parentId,
      topic: "topic" in channel ? (channel.topic ?? null) : null,
    });
  }
  return { roles: guild.roles.cache.map((role) => role.name), channels };
}

function roleByName(guild: Guild, name: string): Role | undefined {
  if (name === EVERYONE) return guild.roles.everyone;
  return guild.roles.cache.find((role) => role.name.toLowerCase() === name.toLowerCase());
}

function flags(perms: ChannelPermission[], value: boolean): Record<string, boolean> {
  return Object.fromEntries(perms.map((perm) => [perm, value]));
}

/** Merges the layout's overwrites into the channel; overwrites for other roles/bits are left as they are. */
async function applyAccess(guild: Guild, channel: GuildChannel, spec: { access: ChannelSpec["access"] }, kind: ChannelKind) {
  for (const overwrite of overwritesFor(spec.access, kind)) {
    const role = roleByName(guild, overwrite.role);
    if (!role) {
      console.warn(`  ! role ${overwrite.role} missing, skipped on ${channel.name}`);
      continue;
    }
    await channel.permissionOverwrites.edit(role, { ...flags(overwrite.allow, true), ...flags(overwrite.deny, false) });
  }
  await channel.permissionOverwrites.edit(guild.members.me!, flags(botOverwrite(kind), true));
}

const CHANNEL_TYPES = { text: ChannelType.GuildText, voice: ChannelType.GuildVoice, forum: ChannelType.GuildForum } as const;

async function run(guild: Guild) {
  const plan = planSetup(snapshot(guild), LAYOUT, ROLES);
  for (const line of describePlan(plan)) console.log(line);
  if (!apply) {
    console.log("\nDry run - nothing changed. Run again with --apply to do it.");
    return;
  }

  for (const role of plan.createRoles) {
    await guild.roles.create({
      name: role.name,
      color: role.color,
      hoist: role.hoist ?? false,
      mentionable: role.mentionable ?? false,
      permissions: role.permissions.map((perm) => PermissionFlagsBits[perm]),
      reason: "Arenacraft server layout",
    });
    console.log(`created role ${role.name}`);
  }

  const categoryOf = new Map<string, CategoryChannel>();
  for (const planned of plan.categories) {
    let category: CategoryChannel;
    if (planned.existing) {
      category = (await guild.channels.fetch(planned.existing.id)) as CategoryChannel;
      if (planned.rename) await category.setName(planned.spec.name);
    } else {
      category = await guild.channels.create({ name: planned.spec.name, type: ChannelType.GuildCategory });
      console.log(`created category ${planned.spec.name}`);
    }
    await applyAccess(guild, category, planned.spec, "text");
    categoryOf.set(planned.spec.name, category);
  }

  const order = new Map<string, string[]>(); // category name -> channel ids in layout order
  for (const planned of plan.channels) {
    const category = categoryOf.get(planned.category.name)!;
    let channel: GuildChannel;
    if (planned.existing) {
      channel = (await guild.channels.fetch(planned.existing.id)) as GuildChannel;
      if (planned.move) await channel.setParent(category, { lockPermissions: false });
      if (planned.setTopic && "setTopic" in channel && typeof channel.setTopic === "function")
        await (channel as GuildChannel & { setTopic(topic: string): Promise<unknown> }).setTopic(planned.spec.topic!);
    } else {
      channel = (await guild.channels.create({
        name: planned.spec.name,
        type: CHANNEL_TYPES[planned.spec.kind],
        parent: category,
        topic: planned.spec.topic,
        availableTags: planned.spec.tags?.map((name) => ({ name })),
      })) as GuildChannel;
      console.log(`created ${planned.spec.kind} ${planned.spec.name}`);
    }
    if (planned.existing && planned.spec.kind === "forum" && planned.spec.tags && channel.type === ChannelType.GuildForum) {
      const forum = channel as unknown as ForumChannel;
      const missing = planned.spec.tags.filter((tag) => !forum.availableTags.some((t) => t.name === tag));
      if (missing.length) await forum.setAvailableTags([...forum.availableTags, ...missing.map((name) => ({ name }))]);
    }
    await applyAccess(guild, channel, planned.spec, planned.spec.kind);
    order.set(planned.category.name, [...(order.get(planned.category.name) ?? []), channel.id]);
  }

  for (const [name, ids] of order) {
    await guild.channels.setPositions(ids.map((id, position) => ({ channel: id, position, parent: categoryOf.get(name)!.id })));
  }
  await guild.channels.setPositions(
    LAYOUT.map((spec, position) => ({ channel: categoryOf.get(spec.name)!.id, position })),
  );

  if (syncPlayers) await givePlayerRole(guild);

  const me = guild.members.me!;
  const highest = Math.max(...ROLES.map((spec) => roleByName(guild, spec.name)?.position ?? 0));
  if (me.roles.highest.position <= highest)
    console.warn(
      "\n! Move the bot's role above the class/language/Player roles (Server Settings -> Roles), " +
        "otherwise it cannot hand them out.",
    );
  console.log("\nDone. Leftover empty categories (if any) can be deleted by hand.");
}

async function givePlayerRole(guild: Guild) {
  const authUrl = authDbUrlFromEnv();
  if (!authUrl) throw new Error("--sync-players needs AUTH_DB_URL (or AC_LOGIN_DATABASE_INFO)");
  const player = roleByName(guild, ROLE_PLAYER)!;
  const authDb = new AuthDb(authUrl);
  let granted = 0;
  try {
    for (const member of (await guild.members.fetch()).values()) {
      if (member.user.bot || member.roles.cache.has(player.id)) continue;
      // accounts are owned by Discord ids (account_discord), not by names
      if (!(await authDb.accountByDiscordId(member.id))) continue;
      await member.roles.add(player, "Already has an Arenacraft account");
      ++granted;
    }
  } finally {
    await authDb.close();
  }
  console.log(`gave ${ROLE_PLAYER} to ${granted} registered member(s)`);
}

const client = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildMembers] });
client.once(Events.ClientReady, async () => {
  try {
    const guild = await client.guilds.fetch(guildId);
    await guild.channels.fetch();
    await guild.roles.fetch();
    await run(guild);
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  } finally {
    await client.destroy();
  }
});
await client.login(token);
