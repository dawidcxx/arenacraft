#pragma once

#include "Types.hpp"

#include <string>
#include <string_view>

class Battleground;

namespace arenacraft::soloq
{
// Short display names used in the match-start composition notice. These are a
// fixed part of the SoloQ UI text, so they live in code rather than in a DBC or
// database lookup.
std::string_view classAbbreviation(Classes classId);
std::string_view specName(Classes classId, uint8_t specIndex);

// "Holy Pala (Healer), Arms War (Melee), Ele Sham (Caster)" - healer first, then
// the two DPS slots. The role label is derived from class/spec, not from the
// team slot, because either DPS slot can hold a melee or caster character.
std::string formatTeamComposition(Team const& team);

// Sends each side the composition of the team it is facing. The enemy team is
// read from the pending SoloQ match for this arena instance, so the text is the
// composition that was actually matched. No-op for non-SoloQ arenas or when the
// match is no longer pending.
void announceEnemyComposition(Battleground* bg);
} // namespace arenacraft::soloq
