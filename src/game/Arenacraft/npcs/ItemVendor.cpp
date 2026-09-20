#include "ItemVendor.hpp"

#include "HunterPets.hpp"
#include "World.h"
#include "WorldSession.h"

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
    // Stablemaster so the hunter-only "Pet Stable" option can drive the
    // client's stable window (CheckStableMaster requires the flag).
    creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER |
                                 UNIT_NPC_FLAG_STABLEMASTER);
}

bool ItemVendor::CanCreatureGossipHello(Player* player, Creature* creature)
{
  if (creature->GetEntry() != Entry)
    return false;

  std::vector<Category> const& categories = Categories();

  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(GossipMenuId);

  // Menu items are kept in a std::map keyed by menu item id, so the ids are
  // assigned in display order. "General goods" is deliberately the first entry.
  uint32 menuIndex = 0;

  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, "General goods", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, GeneralGoodsVendorEntry, 0);
  ++menuIndex;

  for (uint32 i = 0; i < uint32(categories.size()); ++i, ++menuIndex)
  {
    menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, std::string(categories[i].Name), 0, GOSSIP_OPTION_VENDOR, "",
                     0, false);
    menu.AddGossipMenuItemData(menuIndex, VendorEntryBase + i, 0);
  }

  // Glyphs depend on the player's class, so this option points at a
  // class-specific vendor list (stocked in ItemVendorStock::OnStartup).
  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, "Glyphs", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, GlyphVendorEntryBase + player->getClass(), 0);
  ++menuIndex;

  // One shared, slot-ordered enchant list for every player.
  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, "Enchantments", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, EnchantVendorEntry, 0);
  ++menuIndex;

  // Ordinary WotLK epic gems (grouped by colour), then the WotLK meta gems.
  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, "Gems", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, GemVendorEntry, 0);
  ++menuIndex;

  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_VENDOR, "Meta Gems", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, MetaGemVendorEntry, 0);
  ++menuIndex;

  // Hunter-only pet options. "Pet Stable" uses the client-native stable
  // option type and falls through to the core handler; "Get Pet" opens the
  // custom pet family submenu (see HunterPets.hpp).
  if (player->IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET))
  {
    menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_INTERACT_1, "Pet Stable", 0, GOSSIP_OPTION_STABLEPET, "", 0, false);
    menu.AddGossipMenuItemData(menuIndex, 0, 0);
    ++menuIndex;

    menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_INTERACT_1, "Get Pet", 0, UtilGetPet, "", 0, false);
    menu.AddGossipMenuItemData(menuIndex, 0, 0);
    ++menuIndex;
  }

  // Utility actions, handled in CanCreatureGossipSelect.
  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_TRAINER, "Reset Talents", 0, UtilResetTalents, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, 0, 0);
  ++menuIndex;

  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_TRAINER, "Learn Dual Talent Specialization", 0, UtilLearnDualSpec, "",
                   0, false);
  menu.AddGossipMenuItemData(menuIndex, 0, 0);
  ++menuIndex;

  menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_TRAINER, "Learn Spells", 0, UtilLearnSpells, "", 0, false);
  menu.AddGossipMenuItemData(menuIndex, 0, 0);
  ++menuIndex;

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
  return true;
}

bool ItemVendor::CanCreatureGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
  if (creature->GetEntry() != Entry)
    return false;

  switch (action)
  {
  case UtilResetTalents:
    player->PlayerTalkClass->SendCloseGossip();
    if (player->resetTalents(true))
      player->SendTalentsInfoData(false);
    return true;
  case UtilLearnDualSpec:
    player->PlayerTalkClass->SendCloseGossip();
    if (player->GetSpecsCount() == 1 && player->GetLevel() >= sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
    {
      // Same pair the core's trainer gossip option casts.
      player->CastSpell(player, 63680, true, nullptr, nullptr, player->GetGUID());
      player->CastSpell(player, 63624, true, nullptr, nullptr, player->GetGUID());
    }
    return true;
  case UtilLearnSpells:
    // The core serves this from the player's real class trainer list (see
    // ClassTrainer.hpp / NPCHandler.cpp), so only their class's spells exist.
    player->GetSession()->SendTrainerList(creature->GetGUID());
    return true;
  default:
    // Pet submenu actions first, then vendor-list options fall through to the
    // core handler.
    return HandleHunterPetAction(player, creature, action);
  }
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

  // Shared enchant list, opened by the "Enchantments" option.
  for (uint32 item : AllEnchants())
    StockItem(ItemVendor::EnchantVendorEntry, item);

  // Shared gem lists, opened by the "Gems" / "Meta Gems" options.
  for (uint32 item : AllGems())
    StockItem(ItemVendor::GemVendorEntry, item);
  for (uint32 item : AllMetaGems())
    StockItem(ItemVendor::MetaGemVendorEntry, item);

  // Shared general-goods list, opened by the "General goods" option.
  for (uint32 item : AllGeneralGoods())
    StockItem(ItemVendor::GeneralGoodsVendorEntry, item);

  if (CreatureTemplate* proto = const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(ItemVendor::Entry)))
  {
    // Guild-style subtitle so players can tell this is the official gear source.
    proto->SubName = "THE Vendor";

    // "Learn Spells" is served from the player's real class trainer list (see
    // ClassTrainer.hpp). The vendor only needs a non-class trainer_type so the
    // trainer interaction check does not reject it for the player's class; it
    // has no trainer spells of its own.
    proto->trainer_type = TRAINER_TYPE_TRADESKILLS;
  }
}
} // namespace arenacraft
