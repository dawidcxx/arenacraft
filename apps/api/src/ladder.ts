// Pure ladder shaping: database rows in, the JSON the website expects out.

// SharedDefines.h Classes enum.
const CLASS_NAMES: Record<number, string> = {
  1: "Warrior",
  2: "Paladin",
  3: "Hunter",
  4: "Rogue",
  5: "Priest",
  6: "Death Knight",
  7: "Shaman",
  8: "Mage",
  9: "Warlock",
  11: "Druid",
};

export type Role = "Melee" | "Caster" | "Healer";

// arenacraft::soloq::Role, as stored in character_soloq_role.role.
const STORED_ROLES: Record<number, Role> = { 0: "Melee", 1: "Caster", 2: "Healer" };

// Classes with a single soloq role (src/game/Arenacraft/soloq/Roles.cpp), used when a character has
// no stored role yet. Hybrids (paladin, priest, shaman, druid) stay unknown until they queue.
const SINGLE_ROLE_CLASSES: Record<number, Role> = {
  1: "Melee",
  3: "Melee",
  4: "Melee",
  6: "Melee",
  8: "Caster",
  9: "Caster",
};

export interface LadderRow {
  charName: string;
  classId: number;
  storedRole: number | null;
  rating: number;
  seasonWins: number;
  seasonGames: number;
}

// One ladder entry, in the shape the website reads (name, class, role, rating, wins, losses).
export interface LadderEntry {
  name: string;
  class: string;
  role: Role | "";
  rating: number;
  wins: number;
  losses: number;
}

export function roleOf(classId: number, storedRole: number | null): Role | "" {
  if (storedRole !== null && STORED_ROLES[storedRole]) return STORED_ROLES[storedRole];
  return SINGLE_ROLE_CLASSES[classId] ?? "";
}

export function toLadderEntry(row: LadderRow): LadderEntry {
  return {
    name: row.charName,
    class: CLASS_NAMES[row.classId] ?? "Unknown",
    role: roleOf(row.classId, row.storedRole),
    rating: row.rating,
    wins: row.seasonWins,
    losses: Math.max(0, row.seasonGames - row.seasonWins),
  };
}
