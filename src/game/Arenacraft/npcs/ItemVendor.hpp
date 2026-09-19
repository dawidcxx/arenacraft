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
