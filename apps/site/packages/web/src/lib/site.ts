export const DISCORD_URL = "https://discord.gg/tj2DVNQmj2";
export const DISCORD_LABEL = "discord.gg/tj2DVNQmj2";

export const REALM_HOST = "logon.arenacraft.fun";
export const REALMLIST_COMMAND = `set realmlist ${REALM_HOST}`;

export const NEXT_TOURNAMENT = {
  name: "No Warmane november",
  // Sunday 15 November 2026, 18:00 CET.
  startsAt: new Date("2026-11-15T18:00:00+01:00"),
} as const;

export const NAV_LINKS = [
  { label: "Home", to: "/" },
  { label: "Tournaments", to: "/tournaments" },
  { label: "Ladder", to: "/ladder" },
  { label: "Streamer zone", to: "/streamers" },
] as const;