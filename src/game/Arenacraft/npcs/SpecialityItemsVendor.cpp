#include "SpecialityItemsVendor.hpp"

#include "ItemVendor.hpp"
#include "ObjectMgr.h"
#include "ScriptedGossip.h"

#include <string>

namespace arenacraft
{
void SpecialityItemsVendor::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return;

  // GOSSIP drives our menu; VENDOR is required for the armor-list options to
  // open a vendor window; REPAIR is what shows the anvil badge on hover.
  // Replace the DB flags outright so the client cannot fall back to Gahruj's
  // original vendor window.
  creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR);
}

void SpecialityItemsVendor::ShowMainMenu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  // Each speciality category is a submenu; Tier 9 is the only one for now.
  menu.AddMenuItem(0, GOSSIP_ICON_VENDOR, "Tier 9", 0, ActionTier9Menu, "", 0, false);
  menu.AddGossipMenuItemData(0, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

void SpecialityItemsVendor::ShowTier9Menu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  std::vector<Tier9Group> const& groups    = Tier9Groups();
  uint32                          menuIndex = 0;
  for (uint32 i = 0; i < uint32(groups.size()); ++i, ++menuIndex)
  {
    menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, std::string(groups[i].Name), 0, GOSSIP_OPTION_VENDOR, "", 0,
                     false);
    menu.AddGossipMenuItemData(menuIndex, VendorEntryBase + i, 0);
  }

  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_CHAT, "Back", 0, ActionBack, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

bool SpecialityItemsVendor::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  ShowMainMenu(player, creature);
  return true;
}

bool SpecialityItemsVendor::CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/,
                                                    uint32 action)
{
  if (creature->GetEntry() != Entry)
    return false;

  switch (action)
  {
  case ActionTier9Menu:
    ShowTier9Menu(player, creature);
    return true;
  case ActionBack:
    ShowMainMenu(player, creature);
    return true;
  default:
    break;
  }

  // The armor options are GOSSIP_OPTION_VENDOR entries; returning false lets
  // the core open the matching vendor list.
  return false;
}

void SpecialityItemsVendorStock::OnStartup()
{
  std::vector<Tier9Group> const& groups = Tier9Groups();
  for (uint32 i = 0; i < uint32(groups.size()); ++i)
    for (uint32 item : groups[i].Items)
      StockVendorItem(SpecialityItemsVendor::VendorEntryBase + i, item);

  if (CreatureTemplate* proto =
          const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(SpecialityItemsVendor::Entry)))
    proto->SubName = "The Professor";
}
} // namespace arenacraft
