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
// opens a gossip menu with "Exotic mounts" (a vendor list) and "Transmog Item",
// which opens the transmogrification submenus.
class CosmeticVendor : public AllCreatureScript
{
public:
  static constexpr uint32 Entry        = 19728;
  static constexpr uint32 GossipMenuId = 9200001;
  // Vendor list opened by the "Exotic mounts" option.
  static constexpr uint32 ExoticMountVendorEntry = 9200000;

  // Custom gossip actions (outside the built-in Gossip_Option range).
  static constexpr uint32 ActionTransmogById = 9200011;
  static constexpr uint32 ActionClearMenu    = 9200012;
  static constexpr uint32 ActionBack         = 9200013;
  static constexpr uint32 ActionChangeGender = 9200014;
  // One action per equipment slot for the clear menu.
  static constexpr uint32 ActionClearSlotBase = 9200100;

  CosmeticVendor() : AllCreatureScript("arenacraft::CosmeticVendor") {}

  void OnCreatureAddWorld(Creature* creature) override;

  bool CanCreatureGossipHello(Player* player, Creature* creature) override;

  bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;

  bool CanCreatureGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action,
                                   const char* code) override;

private:
  static void ShowMainMenu(Player* player, Creature* creature);
  static void ShowClearMenu(Player* player, Creature* creature);
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
