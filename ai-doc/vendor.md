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

## Multi-vendor (gossip menu)

One NPC can offer several lists: give it `GOSSIP | VENDOR`, show a gossip menu,
and make each entry a `GOSSIP_OPTION_VENDOR` option whose `GossipActionMenuId`
is a *vendor entry* (any unused id). Selecting it calls
`SendListInventory(guid, vendorEntry)`, which reads
`sObjectMgr->GetNpcVendorItemList(vendorEntry)`; stock those entries with
`AddVendorItem` like any other.

Menus can be built without SQL by overriding
`AllCreatureScript::CanCreatureGossipHello`: fill
`player->PlayerTalkClass->GetGossipMenu()` (`AddMenuItem` +
`AddGossipMenuItemData`) and return `true` so the core skips its DB menu. See
`src/game/Arenacraft/npcs/ItemVendor.hpp` for a working example.

### Code-driven stock list (arenacraft multi-vendor)

`src/game/Arenacraft/npcs/` splits the working example into:

- `ItemVendor.hpp` - `ItemVendor` (gossip menu) and `ItemVendorStock`
  (`OnStartup` flush), plus the `ItemEntry` row type.
- `ItemVendor.cpp` - groups the flat list into categories (cached
  `unordered_map<category, vector<itemId>>`) and drives the gossip/stock code.
- `ItemVendorItems.cpp` - the flat `{Category, itemId}` list and
  `CategoryOrder()`. Edit this to add items: every row has a trailing comment
  naming the item. Add a name to `CategoryOrder()` to create a new gossip
  option / vendor list (menu index `i` maps to `VendorEntryBase + i`).

The menu also has a class-dependent **Glyphs** option. It is not part of the
item list: `GlyphVendorItems.cpp` holds a flat `{CLASS_X, itemId}` list (one row
per glyph, grouped/ordered by class, trailing comment naming it) exposed as
`AllGlyphs()`; `ItemVendor::GlyphsForClass(classId)` filters it per class and the
option points at `GlyphVendorEntryBase + classId`. Row order inside a class is
the vendor order. Within each class the glyphs recommended by the Wowhead PvP
arena guides come first (then the rest alphabetically); the guides and their
glyphs are recorded in `ai-doc/class_guides.md`, and the priority block is
regenerated from those notes.

The menu also has a shared **Enchantments** option. `EnchantVendorItems.cpp`
holds a flat `std::vector<uint32>` of permanent-enhancement item ids exposed as
`AllEnchants()`, already ordered by gear slot (head first, weapon last); the
option points at `EnchantVendorEntry`. The list is curated from the Wowhead
WotLK per-spec PvP Arena Season 8 BiS guides, with the per-spec PvE "Enchants
and Gems" guides supplying the remaining general alternatives (all recorded in
`ai-doc/enchant_guides.md`): only recommended, non-profession enchants are kept
(the server has no professions), excluding vanilla/BC filler and strictly
dominated duplicates. Head/shoulder/leg rows are the applied
Arcanum/Inscription/Spellthread/Leg Armor items, the rest are "Scroll of
Enchant ..." consumables.

Finally the menu has three utility options, handled in
`ItemVendor::CanCreatureGossipSelect` (custom action ids; the vendor-list
options fall through to the core handler):

- **Reset Talents** - `Player::resetTalents(true)` (free), then
  `SendTalentsInfoData`.
- **Learn Dual Talent Specialization** - casts 63680/63624 like the core's
  trainer option, if the player has one spec and is at
  `CONFIG_MIN_DUALSPEC_LEVEL` or above.
- **Learn Spells** - `SendTrainerList` on the NPC itself. The NPC is not a real
  trainer: `ItemVendorStock::OnStartup` sets its template's trainer flag and
  copies every class trainer's spell list into it (iterating
  `GetCreatureTemplates()`), and `trainer_type` is set to `TRADESKILLS` so the
  interaction check does not demand a matching `trainer_class`. The core filters
  the merged list per class/race when it is sent, and
  `Player::GetTrainerSpellState` rejects spells that do not fit the buyer, so the
  merge does not leak cross-class spells.

Item ids are checked against wotlk.evowow.com; skip anything tagged
"Not available to players". No `npc_vendor` SQL is involved (`persist = false`).

Current lists: Wrathful Set & Weapons (270 set + 277 weapons), Wrathful Offparts
(264 belts/wrists/rings), Trinkets (264 ICC + 258/245 ToC), ICC Set & Weapons
(264 tier + 264-277 weapons), ICC Offparts (264 belts/wrists/rings) and ICC
Offset (264 non-tier main pieces).

Arena-point items ship with `BuyPrice = 0` (would be free), so `ItemVendorStock`
sets a small fallback gold price on any 0-price stock. `extendedCost = 0` on the
vendor entry is what removes the rating/arena-point requirement.

## Pitfalls

- `.reload npc_vendor` reloads from the DB and drops code-added stock.
- Stock is shared by all players. A genuinely dynamic/per-player item set
  cannot use this list; it needs its own gossip + inventory path.
