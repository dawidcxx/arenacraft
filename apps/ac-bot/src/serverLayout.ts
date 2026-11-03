// The Arenacraft Discord server layout: roles, categories and channels. `scripts/setup-discord.ts` brings the guild
// in line with it (see setupPlan.ts). Existing channels are matched by name (or alias), moved into their category
// and given the access below; they are never deleted or renamed, so message history stays. Channel names the bot
// looks up at runtime (bot, audit-log, Waiting Room, game-<n>-<k>, the vip role) are kept as they are.

export type LanguageCode = "en" | "ru" | "de" | "fr";

export interface Language {
  code: LanguageCode;
  role: string; // role name
  label: string; // button label
  emoji: string;
}

export const LANGUAGES: readonly Language[] = [
  { code: "en", role: "English", label: "English", emoji: "🇬🇧" },
  { code: "ru", role: "Русский", label: "Русский", emoji: "🇷🇺" },
  { code: "de", role: "Deutsch", label: "Deutsch", emoji: "🇩🇪" },
  { code: "fr", role: "Français", label: "Français", emoji: "🇫🇷" },
];

export interface WowClass {
  key: string; // stable id used in component custom ids
  role: string; // role name
  channel: string; // class channel name
  color: number; // in-game class colour
  emoji: string; // guild custom emoji name (shared with the audit log), optional
}

export const CLASSES: readonly WowClass[] = [
  { key: "warrior", role: "Warrior", channel: "warrior", color: 0xc79c6e, emoji: "warrior" },
  { key: "paladin", role: "Paladin", channel: "paladin", color: 0xf58cba, emoji: "paladin" },
  { key: "hunter", role: "Hunter", channel: "hunter", color: 0xabd473, emoji: "hunter" },
  { key: "rogue", role: "Rogue", channel: "rogue", color: 0xfff569, emoji: "rogue" },
  { key: "priest", role: "Priest", channel: "priest", color: 0xf0f0f0, emoji: "priest" },
  { key: "deathknight", role: "Death Knight", channel: "death-knight", color: 0xc41f3b, emoji: "deathknight" },
  { key: "shaman", role: "Shaman", channel: "shaman", color: 0x0070de, emoji: "shaman" },
  { key: "mage", role: "Mage", channel: "mage", color: 0x69ccf0, emoji: "mage" },
  { key: "warlock", role: "Warlock", channel: "warlock", color: 0x9482c9, emoji: "warlock" },
  { key: "druid", role: "Druid", channel: "druid", color: 0xff7d0a, emoji: "druid" },
];

// Guild-wide permissions per role, by discord.js PermissionFlagsBits name.
export type PermissionName =
  | "Administrator"
  | "ManageMessages"
  | "ManageThreads"
  | "ModerateMembers"
  | "KickMembers"
  | "MoveMembers"
  | "MuteMembers"
  | "ViewAuditLog"
  | "ManageEvents"
  | "MentionEveryone";

export interface RoleSpec {
  name: string;
  color?: number;
  hoist?: boolean; // shown separately in the member list
  mentionable?: boolean;
  permissions: PermissionName[];
  existing?: boolean; // only used if present (created elsewhere), never created
}

export const ROLE_ADMIN = "Admin";
export const ROLE_DEVELOPER = "Developer";
export const ROLE_GM = "Game Master";
export const ROLE_PLAYER = "Player";

export const STAFF_ROLES = [ROLE_ADMIN, ROLE_DEVELOPER, ROLE_GM] as const;

export const ROLES: readonly RoleSpec[] = [
  { name: ROLE_ADMIN, color: 0xe74c3c, hoist: true, permissions: ["Administrator"] },
  {
    name: ROLE_DEVELOPER,
    color: 0x9b59b6,
    hoist: true,
    mentionable: true,
    permissions: ["ManageMessages", "ManageThreads", "ModerateMembers", "ViewAuditLog", "ManageEvents"],
  },
  {
    name: ROLE_GM,
    color: 0x2ecc71,
    hoist: true,
    mentionable: true,
    permissions: ["ManageMessages", "ManageThreads", "ModerateMembers", "KickMembers", "MoveMembers", "MuteMembers"],
  },
  { name: ROLE_PLAYER, color: 0x95a5a6, permissions: [] },
  ...CLASSES.map((wowClass): RoleSpec => ({ name: wowClass.role, color: wowClass.color, permissions: [] })),
  ...LANGUAGES.map((language): RoleSpec => ({ name: language.role, permissions: [] })),
];

// Who sees a channel and who writes in it.
export type Access =
  | { kind: "onboarding" } // everyone sees it (also not registered members); only commands, no chatting
  | { kind: "players"; readOnly?: boolean } // registered players (Player role); readOnly: only staff and the bot post
  | { kind: "language"; code: LanguageCode } // members who picked that language
  | { kind: "role"; role: string } // members with that role (e.g. vip)
  | { kind: "staff" }; // Admin, Developer, Game Master

export type ChannelKind = "text" | "voice" | "forum";

export interface ChannelSpec {
  name: string;
  kind: ChannelKind;
  topic?: string;
  access: Access;
  aliases?: string[];
  // Matches every existing channel whose name fits (the dynamic game rooms); never created.
  pattern?: RegExp;
  tags?: string[]; // forum tags
}

export interface CategorySpec {
  name: string;
  aliases?: string[];
  access: Access;
  channels: ChannelSpec[];
}

export const BUG_TAGS = [
  ...CLASSES.map((wowClass) => wowClass.role),
  "Arena / SoloQ",
  "General",
  "Crash",
  "UI / Addon",
  "Open",
  "Confirmed",
  "Fixed",
];

export const BOT_CHANNEL = "bot";
export const SUPPORT_CHANNEL = "support";
export const BUG_FORUM = "bug-reports";

export const LAYOUT: readonly CategorySpec[] = [
  {
    name: "👋 START HERE",
    access: { kind: "onboarding" },
    channels: [
      {
        name: BOT_CHANNEL,
        kind: "text",
        topic: "Start here: pick your language, run /register for your login and realmlist, pick your class.",
        access: { kind: "onboarding" },
      },
    ],
  },
  {
    name: "📢 INFORMATION",
    aliases: ["Kanały tekstowe", "Text Channels"],
    access: { kind: "players", readOnly: true },
    channels: [
      { name: "about", kind: "text", topic: "What Arenacraft is and the server rules.", access: { kind: "players", readOnly: true } },
      { name: "news", kind: "text", topic: "Announcements and maintenance.", access: { kind: "players", readOnly: true } },
      { name: "changelog", kind: "text", topic: "What changed on the server.", access: { kind: "players", readOnly: true } },
      {
        name: "ui-and-links",
        kind: "text",
        topic: "Client, addons, UI packs and useful links.",
        access: { kind: "players", readOnly: true },
      },
    ],
  },
  {
    name: "💬 COMMUNITY",
    access: { kind: "players" },
    channels: [
      { name: "general-discussion", kind: "text", topic: "Talk about anything (English).", access: { kind: "players" } },
      { name: "discussion-ru", kind: "text", topic: "Общение на русском.", access: { kind: "language", code: "ru" } },
      { name: "discussion-de", kind: "text", topic: "Unterhaltung auf Deutsch.", access: { kind: "language", code: "de" } },
      { name: "discussion-fr", kind: "text", topic: "Discussion en français.", access: { kind: "language", code: "fr" } },
      { name: "looking-for-team", kind: "text", topic: "Find partners for 2v2 / 3v3.", access: { kind: "players" } },
      { name: "clips-and-screens", kind: "text", topic: "Your best plays.", access: { kind: "players" } },
      { name: "vip", kind: "text", access: { kind: "role", role: "vip" } },
    ],
  },
  {
    name: "⚔️ ARENA",
    access: { kind: "players" },
    channels: [
      {
        name: "audit-log",
        kind: "text",
        topic: "Solo queue matches: found, finished, rating changes. Posted by the bot.",
        access: { kind: "players", readOnly: true },
      },
      { name: "replays", kind: "text", topic: "Share replay ids worth watching (.replay <id>).", access: { kind: "players" } },
    ],
  },
  {
    name: "🧙 CLASSES",
    access: { kind: "players" },
    channels: CLASSES.map(
      (wowClass): ChannelSpec => ({
        name: wowClass.channel,
        kind: "text",
        topic: `${wowClass.role}: specs, gear, macros, matchups.`,
        access: { kind: "players" },
      }),
    ),
  },
  {
    name: "🛠️ SUPPORT",
    access: { kind: "players" },
    channels: [
      {
        name: SUPPORT_CHANNEL,
        kind: "text",
        topic: "Questions and help. Open a private ticket with the button in the pinned message.",
        access: { kind: "players" },
      },
      {
        name: BUG_FORUM,
        kind: "forum",
        topic: "One post per bug. Use /bug or the button in #support - the form asks for everything we need.",
        access: { kind: "players" },
        tags: BUG_TAGS,
      },
      {
        name: "suggestions",
        kind: "forum",
        topic: "Ideas for the server. One post per idea.",
        access: { kind: "players" },
        tags: ["Gameplay", "Balance", "Discord", "Website", "Other"],
      },
    ],
  },
  {
    name: "🔊 VOICE",
    aliases: ["Kanały głosowe", "Voice Channels"],
    access: { kind: "players" },
    channels: [
      { name: "Chill Room", kind: "voice", access: { kind: "players" } },
      { name: "Waiting Room", kind: "voice", access: { kind: "players" } },
      { name: "game-<n>-<k>", kind: "voice", access: { kind: "players" }, pattern: /^game-(\d+)-(\d+)$/ },
    ],
  },
  {
    name: "🔒 STAFF",
    access: { kind: "staff" },
    channels: [
      { name: "staff-chat", kind: "text", topic: "Admins, developers and game masters.", access: { kind: "staff" } },
      { name: "dev", kind: "text", topic: "Development: deploys, PRs, issues.", access: { kind: "staff" } },
    ],
  },
];
