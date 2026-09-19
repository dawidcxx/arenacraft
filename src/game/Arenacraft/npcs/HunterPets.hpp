#pragma once

#include "Common.h"

class Creature;
class Player;

namespace arenacraft
{
// Gossip action ids of the vendor's "Get Pet" submenu entries, one per pet
// family (see HunterPets.cpp).
constexpr uint32 PetFamilyActionBase = 9001000;

// Opens the "Get Pet" submenu on the vendor: one entry per pet family.
void SendPetFamilyMenu(Player* player, Creature* creature);

// Handles the vendor's "Get Pet" option and the pet family selections. Returns
// true when the action belongs to the pet menu, false to let the core handle
// it (vendor lists, the client-native stable option, ...).
bool HandleHunterPetAction(Player* player, Creature* creature, uint32 action);
} // namespace arenacraft
