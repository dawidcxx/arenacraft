#pragma once

class Player;

namespace arenacraft
{
// Called from Player::Create (before the character enters the world), so a new
// character is already complete when it is first seen - no first-login scripts.
//
// Learns the top-tier armor proficiency the class is entitled to (plate for
// warrior/paladin/death knight, mail for hunter/shaman). The lower tiers come
// from `playercreateinfo_skills`; this is a no-op for the other classes.
void GrantStartingArmorProficiencies(Player* player);

// Learns the weapon proficiencies the class is entitled to and raises the
// matching weapon skills to the level cap.
void GrantStartingWeaponSkills(Player* player);

// Learns the maximum riding skill plus a ground mount directly into the
// spellbook. The mount is a spell, not an item, so it never touches the bags.
void GrantStartingMount(Player* player);

// Raises the First Aid secondary skill to the level cap.
void GrantStartingFirstAid(Player* player);

// Puts the four starting shaman totems into the bags (the totem quests are
// skipped on a level-80-only server). No-op for other classes.
void GrantStartingTotems(Player* player);

// Unlocks every hunter pet stable slot. No-op for other classes.
void GrantStartingStableSlots(Player* player);

// Learns death knight baseline abilities that normally come from the Acherus
// quest line (Horn of Winter rank 1, Runeforging). No-op for other classes.
void GrantStartingDeathKnightSpells(Player* player);

// Learns the class's complete level-80 ability set (every rank) directly into
// the spellbook, so an instant-80 character never has to visit a class trainer.
// Covers trainer-taught abilities plus the auto/quest abilities trainers do not
// carry (hunter pet commands, druid forms, warlock summons, warrior stances,
// ...). Mage teleports/portals are intentionally omitted (PvP-only server).
void GrantStartingSpells(Player* player);
} // namespace arenacraft
