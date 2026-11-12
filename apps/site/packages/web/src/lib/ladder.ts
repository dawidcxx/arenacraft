import type { LadderEntry } from "@arenacraft/common";

export const LADDER_FILTERS = ["All", "Melee", "Caster", "Healer"] as const;
export type LadderFilter = (typeof LADDER_FILTERS)[number];

export const CLASS_COLORS: Record<string, string> = {
  "Death Knight": "#c41e3a",
  Druid: "#ff7c0a",
  Hunter: "#aad372",
  Mage: "#3fc7eb",
  Paladin: "#f48cba",
  Priest: "#ffffff",
  Rogue: "#fff468",
  Shaman: "#0070dd",
  Warlock: "#8788ee",
  Warrior: "#c69b6d",
};

export function winRate(entry: LadderEntry): string {
  const games = entry.wins + entry.losses;
  if (games === 0) return "—";
  return `${Math.round((entry.wins / games) * 100)}%`;
}