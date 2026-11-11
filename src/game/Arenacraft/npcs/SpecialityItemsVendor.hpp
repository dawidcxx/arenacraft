#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "Player.h"
#include "WorldScript.h"

#include <string_view>
#include <vector>

namespace arenacraft
{
// One Tier 9 armor group (Plate/Mail/Leather/Cloth) and the items stocked in
// its vendor list, in display order. Defined in SpecialityItemsVendorItems.cpp.
struct Tier9Group
{
  std::string_view    Name;
  std::vector<uint32> Items;
};

// Flat Tier 9 armor groups, in menu order. Defined in
// SpecialityItemsVendorItems.cpp.
std::vector<Tier9Group> const& Tier9Groups();

// Hijacks Gahruj (entry 20066) into "The Professor", the speciality-items
// vendor. Right-clicking opens a gossip menu with "Tier 9", which opens the
// Plate/Mail/Leather/Cloth list menu; more speciality categories can be added
// alongside it later. The repair flag gives the anvil badge.
class SpecialityItemsVendor : public AllCreatureScript
{
public:
  static constexpr uint32 Entry        = 20066;
  static constexpr uint32 GossipMenuId = 9300001;
  // One vendor list per armor group: VendorEntryBase + group index.
  static constexpr uint32 VendorEntryBase = 9300000;

  // Custom gossip actions (outside the built-in Gossip_Option range).
  static constexpr uint32 ActionTier9Menu = 9300010;
  static constexpr uint32 ActionBack      = 9300011;

  SpecialityItemsVendor() : AllCreatureScript("arenacraft::SpecialityItemsVendor") {}

  void OnCreatureAddWorld(Creature* creature) override;

  bool CanCreatureGossipHello(Player* player, Creature* creature) override;

  bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;

private:
  static void ShowMainMenu(Player* player, Creature* creature);
  static void ShowTier9Menu(Player* player, Creature* creature);
};

// Code-defined stock and subtitle for SpecialityItemsVendor. Runs in OnStartup,
// after ObjectMgr::LoadVendors().
class SpecialityItemsVendorStock : public WorldScript
{
public:
  SpecialityItemsVendorStock()
      : WorldScript("arenacraft::SpecialityItemsVendorStock", {WORLDHOOK_ON_STARTUP})
  {
  }

  void OnStartup() override;
};
} // namespace arenacraft
