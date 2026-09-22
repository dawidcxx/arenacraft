#include "CosmeticVendor.hpp"

#include "Chat.h"
#include "ItemVendor.hpp"
#include "ObjectMgr.h"
#include "ScriptedGossip.h"
#include "StringConvert.h"
#include "transmog/Transmogrification.hpp"

#include <string>

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

void CosmeticVendor::ShowMainMenu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  menu.AddMenuItem(0, GOSSIP_ICON_VENDOR, "Exotic mounts", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(0, ExoticMountVendorEntry, 0);

  // Coded option: the client pops the code-entry box straight away and returns
  // what the player typed (see CanCreatureGossipSelectCode). The box message
  // must stay empty - any text adds a separate "accept/cancel" confirm popup
  // before the code prompt.
  menu.AddMenuItem(1, GOSSIP_ICON_CHAT, "Transmog by item ID", 0, ActionTransmogById, "", 0, true);
  menu.AddGossipMenuItemData(1, 0, 0);

  menu.AddMenuItem(2, GOSSIP_ICON_CHAT, "Clear Transmog", 0, ActionClearMenu, "", 0, false);
  menu.AddGossipMenuItemData(2, 0, 0);

  // Label flips with the player's current gender so it always names the swap.
  bool const toFemale = player->getGender() == GENDER_MALE;
  menu.AddMenuItem(3, GOSSIP_ICON_CHAT, toFemale ? "Change Gender (shnip snap)" : "Change Gender (attach sausage)", 0,
                   ActionChangeGender, "", 0, false);
  menu.AddGossipMenuItemData(3, 0, 0);

  menu.AddMenuItem(4, GOSSIP_ICON_CHAT, "Change Faction", 0, ActionChangeFaction, "", 0, false);
  menu.AddGossipMenuItemData(4, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

void CosmeticVendor::ShowFactionChangeMenu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  menu.AddMenuItem(0, GOSSIP_ICON_INTERACT_1, "Yes, change my faction", 0, ActionChangeFactionConfirm, "", 0, false);
  menu.AddGossipMenuItemData(0, 0, 0);
  menu.AddMenuItem(1, GOSSIP_ICON_CHAT, "No, keep my faction", 0, ActionBack, "", 0, false);
  menu.AddGossipMenuItemData(1, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

void CosmeticVendor::ShowClearMenu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  uint32 index = 0;
  for (transmog::Applied const& applied : transmog::AppliedFor(player))
  {
    menu.AddMenuItem(int32(index), GOSSIP_ICON_CHAT, transmog::EquipmentSlotName(applied.Slot), 0,
                     ActionClearSlotBase + applied.Slot, "", 0, false);
    menu.AddGossipMenuItemData(index, 0, 0);
    ++index;
  }

  menu.AddMenuItem(int32(index), GOSSIP_ICON_TALK, "Back", 0, ActionBack, "", 0, false);
  menu.AddGossipMenuItemData(index, 0, 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

bool CosmeticVendor::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  ShowMainMenu(player, creature);
  return true;
}

bool CosmeticVendor::CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
  if (creature->GetEntry() != Entry)
    return false;

  switch (action)
  {
  case ActionTransmogById: // empty submission: just show the menu again
    ShowMainMenu(player, creature);
    return true;
  case ActionClearMenu:
    if (transmog::AppliedFor(player).empty())
    {
      ChatHandler(player->GetSession()).SendSysMessage("You have no transmogs to clear.");
      ShowMainMenu(player, creature);
    }
    else
      ShowClearMenu(player, creature);
    return true;
  case ActionBack:
    ShowMainMenu(player, creature);
    return true;
  case ActionChangeGender:
  {
    Gender const newGender = player->getGender() == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;
    player->SetByteValue(UNIT_FIELD_BYTES_0, 2, newGender);
    player->SetByteValue(PLAYER_BYTES_3, 0, newGender);
    player->InitDisplayIds();
    ChatHandler(player->GetSession())
        .SendSysMessage(newGender == GENDER_FEMALE ? "You are now female." : "You are now male.");
    ShowMainMenu(player, creature);
    return true;
  }
  case ActionChangeFaction:
    ShowFactionChangeMenu(player, creature);
    return true;
  case ActionChangeFactionConfirm:
  {
    player->PlayerTalkClass->SendCloseGossip();
    player->SetAtLoginFlag(AT_LOGIN_CHANGE_FACTION);
    ChatHandler(player->GetSession())
        .SendSysMessage("Faction change enabled. Log out to the character screen and back in to pick your new race.");
    return true;
  }
  default:
    break;
  }

  if (action >= ActionClearSlotBase && action < ActionClearSlotBase + EQUIPMENT_SLOT_END)
  {
    transmog::ClearSlot(player, uint8(action - ActionClearSlotBase));
    ChatHandler(player->GetSession()).SendSysMessage("Transmog cleared.");
    ShowClearMenu(player, creature);
    return true;
  }

  // "Exotic mounts" is a vendor option; returning false lets the core open the
  // vendor list (see PlayerGossip.cpp).
  return false;
}

bool CosmeticVendor::CanCreatureGossipSelectCode(Player* player, Creature* creature, uint32 /*sender*/, uint32 action,
                                                 const char* code)
{
  if (creature->GetEntry() != Entry || action != ActionTransmogById)
    return false;

  std::string      message;
  Optional<uint32> sourceEntry = Acore::StringTo<uint32>(code ? code : "");
  if (!sourceEntry)
    message = "That is not a valid item id.";
  else if (!transmog::Apply(player, *sourceEntry, message))
    message = "Could not transmogrify: " + message;

  ChatHandler(player->GetSession()).SendSysMessage(message);
  ShowMainMenu(player, creature);
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
