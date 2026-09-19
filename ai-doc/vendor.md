# Vendors

Core reference for how NPC vendors work and how to change them from C++.

## Data model

- `VendorItem` / `VendorItemData` live in
  `src/game/Entities/Creature/CreatureData.h`. A vendor is just an ordered list
  of `{ item, maxcount, incrtime, extendedCost }`.
- `ObjectMgr::_cacheVendorItemStore` (keyed by NPC entry) is the runtime source
  of truth. It is loaded from the `npc_vendor` table by
  `ObjectMgr::LoadVendors()`.
- `Creature::GetVendorItems()` returns `sObjectMgr->GetNpcVendorItemList(GetEntry())`.
  Lists are global per entry — not per spawn, not per player.

## Buying flow

- Right-click sends `CMSG_LIST_INVENTORY` to
  `WorldSession::SendListInventory` (`src/game/Handlers/ItemHandler.cpp`). It
  refuses to interact unless the creature has `UNIT_NPC_FLAG_VENDOR`; the actual
  purchase is `Player::BuyItemFromVendor`.
- `SendListInventory(guid, vendorEntry)` can serve a list keyed to a different
  entry than the creature's own. That overload is the escape hatch for custom
  menus later.

## Making / stocking a vendor from code

- Flag a spawn: `creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_VENDOR)`. Use
  `ReplaceAllNpcFlags`, not `SetNpcFlag`, when the creature already has a
  competing flag: the client prefers QUESTGIVER (2) and opens the quest menu
  instead. `GOSSIP = 1`, `QUESTGIVER = 2`, `VENDOR = 0x80` (`UnitDefines.h`).
  Apply in `AllCreatureScript::OnCreatureAddWorld` so every spawn gets it.
- Add stock: `sObjectMgr->AddVendorItem(entry, item, maxcount, incrtime, extendedCost, persist)`.
  `persist = false` keeps it out of `npc_vendor` (code-defined only).
- Timing: `ScriptMgr::Initialize()` (where script registration runs) fires
  *before* `ObjectMgr::LoadVendors()`. Anything added there is wiped. Register
  code-driven stock from `WorldScript::OnStartup`, which runs after world data
  is loaded.

## Pitfalls

- `.reload npc_vendor` reloads from the DB and drops code-added stock.
- Stock is shared by all players. A genuinely dynamic/per-player item set
  cannot use this list; it needs its own gossip + inventory path.
