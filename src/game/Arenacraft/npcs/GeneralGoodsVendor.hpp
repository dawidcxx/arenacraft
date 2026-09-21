#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "WorldScript.h"

namespace arenacraft
{
// Hijacks Dealer Dunar (entry 20194) into a direct vendor: right-clicking opens
// the vendor window straight away (no gossip), with repair support. Its stock
// is the "General goods" list from GeneralGoodsVendorItems.cpp.
class GeneralGoodsVendor : public AllCreatureScript
{
public:
  static constexpr uint32 Entry = 20194;

  GeneralGoodsVendor() : AllCreatureScript("arenacraft::GeneralGoodsVendor") {}

  void OnCreatureAddWorld(Creature* creature) override;
};

// Code-defined stock and subtitle for GeneralGoodsVendor. Runs in OnStartup,
// after ObjectMgr::LoadVendors().
class GeneralGoodsVendorStock : public WorldScript
{
public:
  GeneralGoodsVendorStock() : WorldScript("arenacraft::GeneralGoodsVendorStock", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override;
};
} // namespace arenacraft
