#pragma once

#include "AllCreatureScript.h"
#include "Creature.h"
#include "GossipDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldScript.h"

#include <string>
#include <string_view>
#include <vector>

namespace arenacraft
{
// One row of the flat vendor stock list (see ItemVendorItems.cpp). `Category`
// picks which vendor list the item is stocked in; the menu/vendor order comes
// from CategoryOrder().
struct ItemEntry
{
  std::string_view Category;
  uint32           ItemId;
};

// Flat stock list, defined in ItemVendorItems.cpp. Add rows there to stock
// items; no SQL involved.
std::vector<ItemEntry> const& AllItems();

// One row of the flat glyph list (see GlyphVendorItems.cpp). `ClassId` is the
// class the glyph belongs to; the "Glyphs" menu option serves one class list.
struct GlyphEntry
{
  uint8  ClassId;
  uint32 ItemId;
};

// Flat glyph stock, defined in GlyphVendorItems.cpp. Row order is the per-class
// vendor order.
std::vector<GlyphEntry> const& AllGlyphs();

// Flat enchant stock, defined in EnchantVendorItems.cpp. One shared list for
// every player, already ordered by gear slot (head first, weapon last).
std::vector<uint32> const& AllEnchants();

// Flat gem stock, defined in GemVendorItems.cpp. AllGems() is the ordinary
// WotLK epic socketables grouped by socket colour (red, blue, yellow, purple,
// orange, green); AllMetaGems() is every WotLK-tier meta gem. One shared list
// each, in vendor order.
std::vector<uint32> const& AllGems();
std::vector<uint32> const& AllMetaGems();

// Flat general-goods stock, defined in GeneralGoodsVendorItems.cpp. One shared
// list of everyday consumables/reagents, in vendor order.
std::vector<uint32> const& AllGeneralGoods();

// Vendor-list (gossip menu) names in display order, defined in
// ItemVendorItems.cpp.
std::vector<std::string_view> const& CategoryOrder();

// Our multi-vendor. Right-clicking opens a gossip menu; every entry opens its
// own vendor list. Both the menu and the stock are derived from AllItems(),
// grouped by Category.
class ItemVendor : public AllCreatureScript
{
public:
  static constexpr uint32 Entry           = 20921;
  static constexpr uint32 GossipMenuId    = 9000001;
  static constexpr uint32 VendorEntryBase = 9000000;
  // Per-class glyph vendor lists, indexed by class id (see GlyphsForClass).
  static constexpr uint32 GlyphVendorEntryBase = 9000100;
  // Shared enchant vendor list (see AllEnchants).
  static constexpr uint32 EnchantVendorEntry = 9000200;
  // Shared gem vendor lists (see AllGems / AllMetaGems).
  static constexpr uint32 GemVendorEntry     = 9000300;
  static constexpr uint32 MetaGemVendorEntry = 9000301;
  // Shared general-goods vendor list (see AllGeneralGoods).
  static constexpr uint32 GeneralGoodsVendorEntry = 9000400;

  // Custom gossip actions for the utility options. Values only have to be
  // distinct and outside the built-in Gossip_Option range.
  static constexpr uint32 UtilResetTalents  = 9000001;
  static constexpr uint32 UtilLearnDualSpec = 9000002;
  static constexpr uint32 UtilLearnSpells   = 9000003;

  struct Category
  {
    std::string_view    Name;
    std::vector<uint32> Items;
  };

  // AllItems() grouped by category, ordered by CategoryOrder(). Built once and
  // cached; the gossip menu index i maps to VendorEntryBase + i.
  static std::vector<Category> const& Categories();

  // Glyph item ids for the given class, in AllGlyphs() order. Cached; empty for
  // an unknown class.
  static std::vector<uint32> const& GlyphsForClass(uint8 classId);

  ItemVendor() : AllCreatureScript("arenacraft::ItemVendor") {}

  void OnCreatureAddWorld(Creature* creature) override;

  bool CanCreatureGossipHello(Player* player, Creature* creature) override;

  bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override;
};

// Code-defined stock, one vendor list per category. Runs in OnStartup, after
// ObjectMgr::LoadVendors().
class ItemVendorStock : public WorldScript
{
public:
  ItemVendorStock() : WorldScript("arenacraft::ItemVendorStock", {WORLDHOOK_ON_STARTUP}) {}

  void OnStartup() override;
};
} // namespace arenacraft
