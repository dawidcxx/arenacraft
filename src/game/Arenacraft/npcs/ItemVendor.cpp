#include "ItemVendor.hpp"

#include <unordered_map>

namespace arenacraft
{
namespace
{
// Small gold fee (in copper) for stock that otherwise has a 0 BuyPrice.
constexpr int32 VendorFallbackPrice = 10 * 10000; // 10 gold

// Adds one item to a vendor list and gives 0-price stock a small gold fee.
// Wrathful/tier/glyph items ship with BuyPrice 0 (arena points / inscription),
// so without this they would be handed out free. The 0 extended cost is what
// removes the rating/arena-point requirement.
void StockItem(uint32 vendorEntry, uint32 item)
{
  sObjectMgr->AddVendorItem(vendorEntry, item, 0, 0, 0, false);

  if (ItemTemplate* proto = const_cast<ItemTemplate*>(sObjectMgr->GetItemTemplate(item)); proto && proto->BuyPrice == 0)
    proto->BuyPrice = VendorFallbackPrice;
}

// Cache of the flat item list grouped by category: category name -> item ids.
// Built once on first use; Categories() stitches it together in menu order.
std::unordered_map<std::string_view, std::vector<uint32>> const& ItemsByCategory()
{
  static std::unordered_map<std::string_view, std::vector<uint32>> const byCategory = []
  {
    std::unordered_map<std::string_view, std::vector<uint32>> map;
    for (ItemEntry const& entry : AllItems())
      map[entry.Category].push_back(entry.ItemId);
    return map;
  }();
  return byCategory;
}

// AllGlyphs() grouped by class id, preserving the flat list's row order.
std::unordered_map<uint8, std::vector<uint32>> const& GlyphsByClass()
{
  static std::unordered_map<uint8, std::vector<uint32>> const byClass = []
  {
    std::unordered_map<uint8, std::vector<uint32>> map;
    for (GlyphEntry const& entry : AllGlyphs())
      map[entry.ClassId].push_back(entry.ItemId);
    return map;
  }();
  return byClass;
}
} // namespace

std::vector<ItemVendor::Category> const& ItemVendor::Categories()
{
  static std::vector<Category> const categories = []
  {
    std::vector<Category> result;
    auto const&           byCategory = ItemsByCategory();

    for (std::string_view name : CategoryOrder())
    {
      auto it = byCategory.find(name);
      result.push_back({name, it != byCategory.end() ? it->second : std::vector<uint32>{}});
    }
    return result;
  }();
  return categories;
}

std::vector<uint32> const& ItemVendor::GlyphsForClass(uint8 classId)
{
  static std::vector<uint32> const empty;
  auto const&                      byClass = GlyphsByClass();
  auto                             it      = byClass.find(classId);
  return it != byClass.end() ? it->second : empty;
}

void ItemVendor::OnCreatureAddWorld(Creature* creature)
{
  if (creature->GetEntry() == Entry)
    creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR);
}

bool ItemVendor::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  std::vector<Category> const& categories = Categories();

  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  for (uint32 i = 0; i < uint32(categories.size()); ++i)
  {
    menu.AddMenuItem(int32(i), GOSSIP_ICON_VENDOR, std::string(categories[i].Name), 0, GOSSIP_OPTION_VENDOR, "", 0,
                     false);
    menu.AddGossipMenuItemData(i, VendorEntryBase + i, 0);
  }

  // Glyphs depend on the player's class, so this option points at a
  // class-specific vendor list (stocked in ItemVendorStock::OnStartup).
  uint32 const glyphMenuIndex = uint32(categories.size());
  menu.AddMenuItem(int32(glyphMenuIndex), GOSSIP_ICON_VENDOR, "Glyphs", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(glyphMenuIndex, GlyphVendorEntryBase + player->getClass(), 0);

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
  return true;
}

void ItemVendorStock::OnStartup()
{
  std::vector<ItemVendor::Category> const& categories = ItemVendor::Categories();

  for (uint32 i = 0; i < uint32(categories.size()); ++i)
    for (uint32 item : categories[i].Items)
      StockItem(ItemVendor::VendorEntryBase + i, item);

  // One glyph list per class, opened by the class-dependent "Glyphs" option.
  for (uint32 classId = 1; classId < MAX_CLASSES; ++classId)
    for (uint32 item : ItemVendor::GlyphsForClass(uint8(classId)))
      StockItem(ItemVendor::GlyphVendorEntryBase + classId, item);
}
} // namespace arenacraft
