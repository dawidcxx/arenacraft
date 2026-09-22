#include "CosmeticVendor.hpp"

#include "Chat.h"
#include "ItemVendor.hpp"
#include "ObjectMgr.h"
#include "ScriptedGossip.h"

namespace arenacraft
{
void CosmeticVendor::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return;

  // Drop the DB QUESTGIVER flag: the client prefers it and would open the quest
  // menu instead of our gossip menu. The vendor flag is needed so the
  // "Exotic mounts" option can open a list.
  creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR);
}

bool CosmeticVendor::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  menu.AddMenuItem(0, GOSSIP_ICON_VENDOR, "Exotic mounts", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(0, ExoticMountVendorEntry, 0);

  menu.AddMenuItem(1, GOSSIP_ICON_CHAT, "Transmog (WIP)", 0, ActionTransmog, "", 0, false);
  menu.AddGossipMenuItemData(1, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
  return true;
}

bool CosmeticVendor::CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
  if (creature->GetEntry() != Entry)
    return false;

  if (action != ActionTransmog)
    // "Exotic mounts" is a vendor option; returning false lets the core open
    // the vendor list (see PlayerGossip.cpp).
    return false;

  player->PlayerTalkClass->SendCloseGossip();
  ChatHandler(player->GetSession()).SendSysMessage("Transmog is a work in progress - check back soon.");
  return true;
}

void CosmeticVendorStock::OnStartup()
{
  for (uint32 item : AllExoticMounts())
    StockVendorItem(CosmeticVendor::ExoticMountVendorEntry, item);

  if (CreatureTemplate* proto = const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(CosmeticVendor::Entry)))
    proto->SubName = "Cosmetics";
}
} // namespace arenacraft
