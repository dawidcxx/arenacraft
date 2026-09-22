#include "Transmogrification.hpp"

#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "UpdateFields.h"

#include <mutex>
#include <optional>
#include <unordered_map>

namespace arenacraft::transmog
{
namespace
{
// In-memory cache of item GUID -> stored appearance. Items are referenced by
// their global GUID, so entries never collide between players. The cache is
// touched by map-update threads (visible slot updates) and the world thread
// (login/logout), hence the mutex.
struct Entry
{
  uint32 Owner;
  uint32 Fake;
};

std::mutex& StoreMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<uint32, Entry>& Store()
{
  static std::unordered_map<uint32, Entry> store;
  return store;
}

std::optional<Entry> FindEntry(uint32 itemGuid)
{
  std::lock_guard<std::mutex> lock(StoreMutex());
  auto                        it = Store().find(itemGuid);
  if (it == Store().end())
    return std::nullopt;
  return it->second;
}

void StoreEntry(uint32 itemGuid, uint32 owner, uint32 fake)
{
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store()[itemGuid] = Entry{owner, fake};
}

void EraseEntry(uint32 itemGuid)
{
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store().erase(itemGuid);
}

// Overrides the visible-item entry of one equipped slot with the stored
// appearance. Only the look is changed; the real item keeps its stats and set
// bonuses. Never pushes an entry the client cannot resolve.
void ApplyToSlot(Player* player, uint8 slot, Item* item)
{
  if (!player || !item || slot >= EQUIPMENT_SLOT_END)
    return;

  std::optional<Entry> entry = FindEntry(item->GetGUID().GetCounter());
  if (!entry)
    return;

  // Only the owner's look is ever applied: a transmogged item that changes
  // hands must not carry its appearance along.
  if (entry->Owner != player->GetGUID().GetCounter())
    return;

  ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry->Fake);
  if (!proto || proto->DisplayInfoID == 0)
  {
    EraseEntry(item->GetGUID().GetCounter());
    return;
  }

  uint32 field = PLAYER_VISIBLE_ITEM_1_ENTRYID + (uint32(slot) * 2);
  player->SetUInt32Value(field, entry->Fake);
  player->ForceValuesUpdateAtIndex(field);
}
} // namespace

bool IsTransmoggableQuality(uint32 quality)
{
  return quality >= ITEM_QUALITY_RARE && quality <= ITEM_QUALITY_LEGENDARY;
}

uint8 EquipmentSlotForInventoryType(uint32 inventoryType)
{
  switch (inventoryType)
  {
  case INVTYPE_HEAD:
    return EQUIPMENT_SLOT_HEAD;
  case INVTYPE_SHOULDERS:
    return EQUIPMENT_SLOT_SHOULDERS;
  case INVTYPE_BODY:
    return EQUIPMENT_SLOT_BODY;
  case INVTYPE_CHEST:
  case INVTYPE_ROBE:
    return EQUIPMENT_SLOT_CHEST;
  case INVTYPE_WAIST:
    return EQUIPMENT_SLOT_WAIST;
  case INVTYPE_LEGS:
    return EQUIPMENT_SLOT_LEGS;
  case INVTYPE_FEET:
    return EQUIPMENT_SLOT_FEET;
  case INVTYPE_WRISTS:
    return EQUIPMENT_SLOT_WRISTS;
  case INVTYPE_HANDS:
    return EQUIPMENT_SLOT_HANDS;
  case INVTYPE_CLOAK:
    return EQUIPMENT_SLOT_BACK;
  case INVTYPE_TABARD:
    return EQUIPMENT_SLOT_TABARD;
  case INVTYPE_WEAPON:
  case INVTYPE_WEAPONMAINHAND:
  case INVTYPE_2HWEAPON:
    return EQUIPMENT_SLOT_MAINHAND;
  case INVTYPE_WEAPONOFFHAND:
  case INVTYPE_SHIELD:
  case INVTYPE_HOLDABLE:
    return EQUIPMENT_SLOT_OFFHAND;
  case INVTYPE_RANGED:
  case INVTYPE_RANGEDRIGHT:
  case INVTYPE_THROWN:
    return EQUIPMENT_SLOT_RANGED;
  default:
    return EQUIPMENT_SLOT_END;
  }
}

bool SlotAcceptsInventoryType(uint8 slot, uint32 inventoryType)
{
  uint8 primary = EquipmentSlotForInventoryType(inventoryType);
  if (primary == EQUIPMENT_SLOT_END)
    return false;

  if (primary == slot)
    return true;

  // A plain one-hand weapon can be worn in either hand.
  if (inventoryType == INVTYPE_WEAPON && (slot == EQUIPMENT_SLOT_MAINHAND || slot == EQUIPMENT_SLOT_OFFHAND))
    return true;

  return false;
}

char const* EquipmentSlotName(uint8 slot)
{
  switch (slot)
  {
  case EQUIPMENT_SLOT_HEAD:
    return "Head";
  case EQUIPMENT_SLOT_NECK:
    return "Neck";
  case EQUIPMENT_SLOT_SHOULDERS:
    return "Shoulders";
  case EQUIPMENT_SLOT_BODY:
    return "Shirt";
  case EQUIPMENT_SLOT_CHEST:
    return "Chest";
  case EQUIPMENT_SLOT_WAIST:
    return "Waist";
  case EQUIPMENT_SLOT_LEGS:
    return "Legs";
  case EQUIPMENT_SLOT_FEET:
    return "Feet";
  case EQUIPMENT_SLOT_WRISTS:
    return "Wrists";
  case EQUIPMENT_SLOT_HANDS:
    return "Hands";
  case EQUIPMENT_SLOT_FINGER1:
  case EQUIPMENT_SLOT_FINGER2:
    return "Ring";
  case EQUIPMENT_SLOT_TRINKET1:
  case EQUIPMENT_SLOT_TRINKET2:
    return "Trinket";
  case EQUIPMENT_SLOT_BACK:
    return "Back";
  case EQUIPMENT_SLOT_MAINHAND:
    return "Main Hand";
  case EQUIPMENT_SLOT_OFFHAND:
    return "Off Hand";
  case EQUIPMENT_SLOT_RANGED:
    return "Ranged";
  case EQUIPMENT_SLOT_TABARD:
    return "Tabard";
  default:
    return "Unknown";
  }
}

void LoadPlayer(Player* player)
{
  if (!player)
    return;

  uint32 owner = player->GetGUID().GetCounter();

  // The join with item_instance drops rows for items that no longer exist.
  QueryResult result = CharacterDatabase.Query("SELECT ct.ItemGuid, ct.FakeEntry FROM character_transmogrification ct "
                                               "JOIN item_instance ii ON ii.guid = ct.ItemGuid WHERE ct.OwnerGuid = {}",
                                               owner);
  if (result)
  {
    do
    {
      uint32 itemGuid  = (*result)[0].Get<uint32>();
      uint32 fakeEntry = (*result)[1].Get<uint32>();
      if (sObjectMgr->GetItemTemplate(fakeEntry))
        StoreEntry(itemGuid, owner, fakeEntry);
    } while (result->NextRow());
  }

  // The visible slots were filled during item load, before this cache existed;
  // re-apply the appearances now.
  for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
      ApplyToSlot(player, slot, item);
}

void UnloadPlayer(Player* player)
{
  if (!player)
    return;

  uint32 owner = player->GetGUID().GetCounter();

  std::lock_guard<std::mutex> lock(StoreMutex());
  for (auto it = Store().begin(); it != Store().end();)
  {
    if (it->second.Owner == owner)
      it = Store().erase(it);
    else
      ++it;
  }
}

bool Apply(Player* player, uint32 sourceEntry, std::string& message)
{
  if (!player)
    return false;

  ItemTemplate const* source = sObjectMgr->GetItemTemplate(sourceEntry);
  if (!source)
  {
    message = "Unknown item id.";
    return false;
  }

  if (source->Class != ITEM_CLASS_ARMOR && source->Class != ITEM_CLASS_WEAPON)
  {
    message = "Only armor and weapons can be used as appearances.";
    return false;
  }

  if (!IsTransmoggableQuality(source->Quality))
  {
    message = "Only rare, epic and legendary items can be used as appearances.";
    return false;
  }

  if (source->DisplayInfoID == 0)
  {
    message = "That item has no appearance.";
    return false;
  }

  for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
  {
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item)
      continue;

    ItemTemplate const* target = item->GetTemplate();

    // Same slot, same class and same armor/weapon type; both must be rare+.
    if (!SlotAcceptsInventoryType(slot, source->InventoryType))
      continue;
    if (target->Class != source->Class || target->SubClass != source->SubClass)
      continue;
    if (!IsTransmoggableQuality(target->Quality))
      continue;
    if (target->ItemId == source->ItemId || target->DisplayInfoID == source->DisplayInfoID)
      continue;

    uint32 owner    = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();
    StoreEntry(itemGuid, owner, sourceEntry);
    CharacterDatabase.Execute(
        "REPLACE INTO character_transmogrification (ItemGuid, OwnerGuid, FakeEntry) VALUES ({}, {}, {})", itemGuid,
        owner, sourceEntry);
    ApplyToSlot(player, slot, item);

    message = "Transmogrified your " + std::string(EquipmentSlotName(slot)) + " into " + source->Name1 + ".";
    return true;
  }

  message = "No compatible equipped item found for that appearance.";
  return false;
}

void ClearSlot(Player* player, uint8 slot)
{
  if (!player || slot >= EQUIPMENT_SLOT_END)
    return;

  Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
  if (!item)
    return;

  EraseEntry(item->GetGUID().GetCounter());
  CharacterDatabase.Execute("DELETE FROM character_transmogrification WHERE ItemGuid = {}",
                            item->GetGUID().GetCounter());

  // Rewrites the real entry and (via the player script hook) leaves it alone,
  // since the cache no longer has an appearance for this item.
  player->SetVisibleItemSlot(slot, item);
}

std::vector<Applied> AppliedFor(Player* player)
{
  std::vector<Applied> applied;
  if (!player)
    return applied;

  for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
  {
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item)
      continue;

    if (std::optional<Entry> entry = FindEntry(item->GetGUID().GetCounter());
        entry && entry->Owner == player->GetGUID().GetCounter())
      applied.push_back(Applied{slot, entry->Fake});
  }

  return applied;
}

TransmogPlayerScript::TransmogPlayerScript()
    : PlayerScript("arenacraft::transmog::TransmogPlayerScript",
                   {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_AFTER_SET_VISIBLE_ITEM_SLOT})
{
}

void TransmogPlayerScript::OnLogin(Player* player) { LoadPlayer(player); }

void TransmogPlayerScript::OnLogout(Player* player) { UnloadPlayer(player); }

void TransmogPlayerScript::OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item)
{
  ApplyToSlot(player, slot, item);
}
} // namespace arenacraft::transmog
