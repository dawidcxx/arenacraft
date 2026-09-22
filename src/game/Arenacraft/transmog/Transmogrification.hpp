#pragma once

#include "Player.h"
#include "PlayerScript.h"

#include <string>
#include <vector>

namespace arenacraft::transmog
{
// One equipped item that currently shows a transmogrified appearance.
struct Applied
{
  uint8  Slot;        // EquipmentSlots
  uint32 SourceEntry; // item id whose look is shown
};

// --- Pure helpers (no world state; covered by unit tests) --------------------

// Only rare, epic and legendary appearances may be used ("no trashmogs").
bool IsTransmoggableQuality(uint32 quality);

// The equipment slot an inventory type occupies, or EQUIPMENT_SLOT_END when the
// type cannot be transmogrified.
uint8 EquipmentSlotForInventoryType(uint32 inventoryType);

// Whether an item with the given inventory type can be worn in the slot.
bool SlotAcceptsInventoryType(uint8 slot, uint32 inventoryType);

// Display name for an equipment slot (English; this server is enUS only).
char const* EquipmentSlotName(uint8 slot);

// --- World-state API ---------------------------------------------------------

// Loads the player's stored appearances and re-applies them to the visible
// equipment. Runs on login, after the character's items are loaded.
void LoadPlayer(Player* player);

// Drops the player's entries from the in-memory cache. Runs on logout.
void UnloadPlayer(Player* player);

// Applies the look of `sourceEntry` to the first compatible equipped item.
// Returns false and fills `message` when nothing was applied.
bool Apply(Player* player, uint32 sourceEntry, std::string& message);

// Removes the transmog from the item in the given equipment slot.
void ClearSlot(Player* player, uint8 slot);

// Equipped items that currently have a transmog, in slot order.
std::vector<Applied> AppliedFor(Player* player);

// Keeps the fake appearance in sync with the player's visible items.
class TransmogPlayerScript : public PlayerScript
{
public:
  TransmogPlayerScript();

  void OnLogin(Player* player) override;
  void OnLogout(Player* player) override;
  void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override;
};
} // namespace arenacraft::transmog
