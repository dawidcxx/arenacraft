#pragma once

#include "Define.h"

#include <optional>

// Autocast choices for guardian pets (Feral Spirit wolves, the death knight's ghoul without Master of Ghouls, treants,
// ...). Real pets (hunter, warlock) save their autocast with the pet; guardians are created fresh on every summon and
// got the default (autocast on) back each time. The choice a player makes on the pet bar is remembered per character,
// creature entry and spell, kept in the characters DB (character_pet_autocast) and applied on every summon.
namespace arenacraft::petautocast
{
// World thread, at startup.
void Load();

// The player's choice for `spell` of guardian `entry`, if they ever toggled it.
std::optional<bool> Choice(uint32 owner, uint32 entry, uint32 spell);

// Remembers a toggle (stored right away).
void Remember(uint32 owner, uint32 entry, uint32 spell, bool enabled);

// Registers the startup loader.
void AddPetAutocastScripts();
} // namespace arenacraft::petautocast
