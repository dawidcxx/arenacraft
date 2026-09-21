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

**General goods** is not a gossip option on the multi-vendor. It is a separate,
standalone direct vendor: entry 20194 ("Dealer Dunar") is hijacked by
`GeneralGoodsVendor`/`GeneralGoodsVendorStock` (`GeneralGoodsVendor.hpp/.cpp`) -
`OnCreatureAddWorld` replaces its flags with `VENDOR | REPAIR` (no gossip, so a
right-click goes straight to the vendor window) and `OnStartup` clears the
`npc_vendor` stock the DB ships for the entry, stocks `AllGeneralGoods()`, and
sets its subname to "General goods". `GeneralGoodsVendorItems.cpp` holds that
list: a flat `std::vector<uint32>` of everyday consumables/reagents (conjured
mage food, Soul Shard, ammo, rogue poisons, class reagents, the best WotLK
bandage). Some entries are conjured and tagged "Not available to players" on
evowow, but are intentionally stocked anyway. Shared stock helper:
`StockVendorItem` (`ItemVendor.hpp`).

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

The menu then has two gem options. `GemVendorItems.cpp` holds two flat
`std::vector<uint32>` lists exposed as `AllGems()` and `AllMetaGems()`, served by
the **Gems** and **Meta Gems** options. `AllGems()` is the ordinary WotLK epic
socketables - the cut gems of the six WotLK epic families (Cardinal Ruby,
Majestic Zircon, King's Amber, Dreadstone, Ametrine, Eye of Zul) - grouped by
socket colour in that order. `AllMetaGems()` is every WotLK-tier (level 80) meta
gem. Both are curated from the wotlk.evowow.com gem listings (`?items=3` for the
coloured gems, `?items=3.6` for the metas): only quality-epic WotLK gems are
kept (rare WotLK gems, all BC-era gems and all other pre-WotLK gems are
skipped), and the special non-ordinary WotLK gems (jewelcrafting-only unique
Dragon's Eye cuts, Stormjewel, Kharmaa's Grace, prismatic Nightmare Tear) are
excluded. Every id was checked available to players on evowow.

Hunters also get two pet options (see `ai-doc/hunter_pets.md`): **Pet Stable**
(the client-native `GOSSIP_OPTION_STABLEPET` option type, which falls through to
the core and needs the `UNIT_NPC_FLAG_STABLEMASTER` flag the vendor carries) and
**Get Pet**, which opens a custom pet family submenu (`HunterPets.hpp/.cpp`,
actions `PetFamilyActionBase + i`).

Finally the menu has three utility options, handled in
`ItemVendor::CanCreatureGossipSelect` (custom action ids; the vendor-list
options fall through to the core handler):

- **Reset Talents** - `Player::resetTalents(true)` (free), then
  `SendTalentsInfoData`.
- **Learn Dual Talent Specialization** - casts 63680/63624 like the core's
  trainer option, if the player has one spec and is at
  `CONFIG_MIN_DUALSPEC_LEVEL` or above.
- **Learn Spells** - `SendTrainerList` on the vendor itself, but the core is
  patched so the trainer data comes from the player's *real* class trainer list
  (`arenacraft::ClassTrainerSpellsFor`, `src/game/Arenacraft/ClassTrainer.cpp`):
  `WorldSession::SendTrainerList` and `HandleTrainerBuySpellOpcode` ask
  `ClassTrainerSpellsFor(unit, player)` first and only fall back to
  `unit->GetTrainerSpells()`. Because the served list only ever contains the
  player's class spells, another class's spells can never appear (a merged
  all-class list leaked them, so don't go back to that). The vendor has the
  trainer flag but `trainer_type = TRADESKILLS` (set in `OnStartup`) so the
  interaction check does not reject it for the player's class and it has no
  trainer spells of its own.

Item ids are checked against wotlk.evowow.com; skip anything tagged
"Not available to players". No `npc_vendor` SQL is involved (`persist = false`).
Soul Shards (`6265`) are made stackable (stack size 20) by the world migration
`data/sql/updates/db_world/2026_09_19_00.sql` (applied by `scripts/db_sync`).

The vendor creature (entry 20921 "Shauly Pore") gets a guild-style `subname`
("<THE Vendor>", set in `ItemVendorStock::OnStartup`) so players recognise it as
the official gear source; NPCs cannot carry a real guild.

Current lists: Wrathful Set & Weapons (270 set + 277 weapons), Wrathful Offparts
(264 belts/feet/wrists/rings/necks/cloaks, plus the two Relentless Gladiator
rings), Trinkets (264 ICC + 258/245 ToC + the five ilvl-245 Battlemaster PvP
trinkets), ICC Set & Weapons (264 tier + 277
weapons; the weaker 264/271 PvE weapon drops are not stocked, except the 264
thrown Gluth's Fetching Knife, which has no 277 version), ICC Offparts (264
necks/cloaks/boots/belts/wrists/rings plus the 272 ToGC tribute-chest cloaks) and
ICC Offset (264 non-tier main pieces).

Arena-point items ship with `BuyPrice = 0` (would be free), so `ItemVendorStock`
sets a small fallback gold price on any 0-price stock. `extendedCost = 0` on the
vendor entry is what removes the rating/arena-point requirement.

## Pitfalls

- `.reload npc_vendor` reloads from the DB and drops code-added stock.
- Stock is shared by all players. A genuinely dynamic/per-player item set
  cannot use this list; it needs its own gossip + inventory path.
