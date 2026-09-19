#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"

namespace arenacraft::vendors
{
constexpr uint32 SHAULY_PORE_ENTRY = 20921;

// Gives Shauly Pore the vendor flag so he opens an (empty) vendor window.
// Items can be added later through the npc_vendor table.
class ShaulyPoreVendor : public AllCreatureScript
{
public:
  ShaulyPoreVendor() : AllCreatureScript("ShaulyPoreVendor") {}

  void OnCreatureAddWorld(Creature* creature) override
  {
    if (creature->GetEntry() == SHAULY_PORE_ENTRY)
      creature->SetNpcFlag(UNIT_NPC_FLAG_VENDOR);
  }
};
} // namespace arenacraft::vendors
