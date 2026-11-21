#pragma once

#include "ObjectGuid.h"

#include <string>

class ArenaTeam;
class Player;

namespace arenacraft::solo2v2
{
// The character's own 2v2 ArenaTeam, or nullptr when it has none.
ArenaTeam* FindTeam(ObjectGuid guid);

// Returns the character's solo 2v2 team, creating it at the starting rating/MMR when missing. Works for offline
// characters as long as the character cache knows them. nullptr on failure.
ArenaTeam* EnsureTeam(ObjectGuid guid);

// Resets the team's rating and games/wins to the starting values, keeping the member's MMR (like the SoloQ reset).
bool ResetTeam(Player* player);

// Renames the character's team after a character rename.
void OnCharacterRenamed(ObjectGuid guid, std::string const& newName);
} // namespace arenacraft::solo2v2
