export const DISCORD_URL = "https://discord.gg/tj2DVNQmj2";
export const DISCORD_LABEL = "discord.gg/tj2DVNQmj2";

export const REALM_HOST = "logon.arenacraft.fun";
export const REALMLIST_COMMAND = `set realmlist ${REALM_HOST}`;

export const NEXT_TOURNAMENT = {
  name: "No Warmane november",
  // Sunday 15 November 2026, 18:00-21:00 CET.
  startsAt: new Date("2026-11-15T18:00:00+01:00"),
  endsAt: new Date("2026-11-15T21:00:00+01:00"),
  windowLabel: "18:00–21:00 CET",
} as const;

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
