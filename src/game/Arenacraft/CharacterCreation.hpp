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
} // namespace arenacraft
