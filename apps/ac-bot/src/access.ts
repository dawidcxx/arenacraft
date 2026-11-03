import type { Access, ChannelKind } from "./serverLayout";
import { LANGUAGES, ROLE_PLAYER, STAFF_ROLES } from "./serverLayout";

// Channel permission overwrites per access rule, by role name ("@everyone" for the everyone role) and
// discord.js PermissionFlagsBits names. scripts/setup-discord.ts merges them into each channel: only these bits
// are set, every other overwrite the channel already has stays (e.g. the bot's Speak deny in the Waiting Room).

export type ChannelPermission =
  | "ViewChannel"
  | "SendMessages"
  | "SendMessagesInThreads"
  | "CreatePublicThreads"
  | "CreatePrivateThreads"
  | "ReadMessageHistory"
  | "UseApplicationCommands"
  | "AddReactions"
  | "Connect";

export interface Overwrite {
  role: string;
  allow: ChannelPermission[];
  deny: ChannelPermission[];
}

export const EVERYONE = "@everyone";

function staffSees(kind: ChannelKind): Overwrite[] {
  const allow: ChannelPermission[] = kind === "voice" ? ["ViewChannel", "Connect"] : ["ViewChannel", "SendMessages"];
  return STAFF_ROLES.map((role) => ({ role, allow, deny: [] }));
}

export function overwritesFor(access: Access, kind: ChannelKind): Overwrite[] {
  const see: ChannelPermission[] = kind === "voice" ? ["ViewChannel", "Connect"] : ["ViewChannel"];

  switch (access.kind) {
    case "onboarding":
      // Everyone sees it and runs commands; chatting stays off so the instructions remain readable.
      return [
        {
          role: EVERYONE,
          allow: ["ViewChannel", "ReadMessageHistory", "UseApplicationCommands"],
          deny: ["SendMessages", "CreatePublicThreads", "CreatePrivateThreads"],
        },
        ...staffSees(kind),
      ];
    case "players":
      return [
        { role: EVERYONE, allow: [], deny: ["ViewChannel"] },
        access.readOnly
          ? { role: ROLE_PLAYER, allow: see, deny: ["SendMessages", "CreatePublicThreads"] }
          : { role: ROLE_PLAYER, allow: see, deny: [] },
        ...staffSees(kind),
      ];
    case "language": {
      const language = LANGUAGES.find((l) => l.code === access.code);
      if (!language) throw new Error(`unknown language ${access.code}`);
      return [{ role: EVERYONE, allow: [], deny: ["ViewChannel"] }, { role: language.role, allow: see, deny: [] }, ...staffSees(kind)];
    }
    case "role":
      return [{ role: EVERYONE, allow: [], deny: ["ViewChannel"] }, { role: access.role, allow: see, deny: [] }, ...staffSees(kind)];
    case "staff":
      return [{ role: EVERYONE, allow: [], deny: ["ViewChannel"] }, ...staffSees(kind)];
  }
}

/** Permissions the bot itself keeps on every managed channel, so hiding a channel never locks the bot out. */
export function botOverwrite(kind: ChannelKind): ChannelPermission[] {
  if (kind === "voice") return ["ViewChannel", "Connect"];
  return [
    "ViewChannel",
    "SendMessages",
    "SendMessagesInThreads",
    "CreatePublicThreads",
    "CreatePrivateThreads",
    "ReadMessageHistory",
  ];
}
