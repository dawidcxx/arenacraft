#pragma once

class Player;

namespace arenacraft
{
// Called from Player::Create (before the character enters the world), so a new
// character is already complete when it is first seen - no first-login scripts.
//
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
} // namespace arenacraft
