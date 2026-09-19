#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "WorldScript.h"

namespace arenacraft::vendors
{
constexpr uint32 ShaulyPoreEntry = 20921;
constexpr uint32 DirgeItem       = 23555;

// Shauly Pore ships as a quest giver (npcflag 2). The client prefers the
// questgiver interaction, so *replace* his flags rather than adding VENDOR on
// top, otherwise right-click still opens the quest menu.
class ShaulyPoreVendor : public AllCreatureScript
{
public:
  ShaulyPoreVendor() : AllCreatureScript("arenacraft::vendors::ShaulyPoreVendor") {}

  void OnCreatureAddWorld(Creature* creature) override
  {
    if (creature->GetEntry() == ShaulyPoreEntry)
      creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_VENDOR);
  }
};

// Stock is declared in code instead of the `npc_vendor` table so it can be
// swapped for a dynamic item set later. Has to run in OnStartup, which fires
// after ObjectMgr::LoadVendors().
class VendorStock : public WorldScript
{
public:
  VendorStock() : WorldScript("arenacraft::vendors::VendorStock", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override { AddItem(ShaulyPoreEntry, DirgeItem); }

private:
  static void AddItem(uint32 npcEntry, uint32 item, int32 maxCount = 0, uint32 incrTime = 0, uint32 extendedCost = 0)
  {
    sObjectMgr->AddVendorItem(npcEntry, item, maxCount, incrTime, extendedCost, false);
  }
};
} // namespace arenacraft::vendors
