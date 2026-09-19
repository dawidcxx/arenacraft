#pragma once

#include "Define.h"

class Creature;
class Player;
struct TrainerSpellData;

namespace arenacraft
{
// Creature entry of a real class trainer that has the class's complete spell
// list, or 0 for an unknown class.
uint32 ClassTrainerEntry(uint8 classId);

// Trainer spell list to serve for `unit`: when `unit` is the arenacraft
// multi-vendor, the real class trainer's list for the player's class;
// otherwise nullptr (callers fall back to unit->GetTrainerSpells()). This is
// what keeps "Learn Spells" from ever exposing another class's spells.
TrainerSpellData const* ClassTrainerSpellsFor(Creature const* unit, Player const* player);
} // namespace arenacraft
