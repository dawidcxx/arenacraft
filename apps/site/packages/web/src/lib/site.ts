export const DISCORD_URL = "https://discord.gg/tj2DVNQmj2";
export const DISCORD_LABEL = "discord.gg/tj2DVNQmj2";

export const REALM_HOST = "logon.arenacraft.fun";
export const REALMLIST_COMMAND = `set realmlist ${REALM_HOST}`;

export type NavLinkItem = { label: string; to: string };

/** Always visible in the header. */
export const NAV_LINKS: readonly NavLinkItem[] = [
  { label: "Home", to: "/" },
  { label: "Ladder", to: "/ladder" },
];

/** Under the header's "More" dropdown. */
export const NAV_MORE_LINKS: readonly NavLinkItem[] = [
  { label: "Tournaments", to: "/tournaments" },
  { label: "Armory", to: "/armory" },
  { label: "Streamer zone", to: "/streamers" },
  { label: "Addons", to: "/addons" },
  { label: "Hall of fame", to: "/hall-of-fame" },
];
