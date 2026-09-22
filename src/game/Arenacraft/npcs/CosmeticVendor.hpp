#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "Player.h"
#include "WorldScript.h"

#include <vector>

namespace arenacraft
{
// Flat exotic-mount stock, defined in CosmeticVendorItems.cpp. Row order is the
// vendor order; add the real mount ids there.
std::vector<uint32> const& AllExoticMounts();

// Hijacks Mama Wheeler (entry 19728) into the cosmetics NPC. Right-clicking
// opens a gossip menu with "Exotic mounts" (a vendor list) and a placeholder
// "Transmog (WIP)" option until the transmog feature lands.
class CosmeticVendor : public AllCreatureScript
{
public:
  static constexpr uint32 Entry        = 19728;
  static constexpr uint32 GossipMenuId = 9200001;
  // Vendor list opened by the "Exotic mounts" option.
  static constexpr uint32 ExoticMountVendorEntry = 9200000;
  // Custom action for the transmog placeholder (outside the built-in range).
  static constexpr uint32 ActionTransmog = 9200010;

  CosmeticVendor() : AllCreatureScript("arenacraft::CosmeticVendor") {}

  void OnCreatureAddWorld(Creature* creature) override;

  bool CanCreatureGossipHello(Player* player, Creature* creature) override;

  bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;
};

// Code-defined stock for the "Exotic mounts" list. Runs in OnStartup, after
// ObjectMgr::LoadVendors().
class CosmeticVendorStock : public WorldScript
{
public:
  CosmeticVendorStock() : WorldScript("arenacraft::CosmeticVendorStock", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override;
};
} // namespace arenacraft
