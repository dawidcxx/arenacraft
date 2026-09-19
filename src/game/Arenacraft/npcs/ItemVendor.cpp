#include "ItemVendor.hpp"

#include "SpellMgr.h"
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
    creature->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER);
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

  // One shared, slot-ordered enchant list for every player.
  uint32 const enchantMenuIndex = glyphMenuIndex + 1;
  menu.AddMenuItem(int32(enchantMenuIndex), GOSSIP_ICON_VENDOR, "Enchantments", 0, GOSSIP_OPTION_VENDOR, "", 0, false);
  menu.AddGossipMenuItemData(enchantMenuIndex, EnchantVendorEntry, 0);

  // Utility actions, handled in CanCreatureGossipSelect.
  uint32 const resetTalentsIndex = enchantMenuIndex + 1;
  menu.AddMenuItem(int32(resetTalentsIndex), GOSSIP_ICON_TRAINER, "Reset Talents", 0, UtilResetTalents, "", 0, false);
  menu.AddGossipMenuItemData(resetTalentsIndex, 0, 0);

  uint32 const dualSpecIndex = resetTalentsIndex + 1;
  menu.AddMenuItem(int32(dualSpecIndex), GOSSIP_ICON_TRAINER, "Learn Dual Talent Specialization", 0, UtilLearnDualSpec,
                   "", 0, false);
  menu.AddGossipMenuItemData(dualSpecIndex, 0, 0);

  uint32 const learnSpellsIndex = dualSpecIndex + 1;
  menu.AddMenuItem(int32(learnSpellsIndex), GOSSIP_ICON_TRAINER, "Learn Spells", 0, UtilLearnSpells, "", 0, false);
  menu.AddGossipMenuItemData(learnSpellsIndex, 0, 0);

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
    player->GetSession()->SendTrainerList(creature->GetGUID());
    return true;
  default:
    // Vendor-list options fall through to the core handler.
    return false;
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

  // Merge every class trainer's spell list into this NPC so "Learn Spells" can
  // open the player's class trainer without a real trainer creature. The core
  // filters the merged list down to the player's class/race when it is sent.
  if (CreatureTemplate* proto = const_cast<CreatureTemplate*>(sObjectMgr->GetCreatureTemplate(ItemVendor::Entry)))
  {
    proto->npcflag |= UNIT_NPC_FLAG_TRAINER;
    // Deliberately not TRAINER_TYPE_CLASS: the interaction check would then
    // demand trainer_class match the player, which one shared NPC cannot do
    // for every class.
    proto->trainer_type = TRAINER_TYPE_TRADESKILLS;

    uint32 mergedSpells = 0;
    for (auto const& [trainerEntry, trainer] : *sObjectMgr->GetCreatureTemplates())
    {
      if (trainer.trainer_type != TRAINER_TYPE_CLASS || trainer.trainer_class == 0)
        continue;

      TrainerSpellData const* spells = sObjectMgr->GetNpcTrainerSpells(trainerEntry);
      if (!spells)
        continue;

      for (auto const& entry : spells->spellList)
      {
        TrainerSpell const& trainerSpell = entry.second;
        sObjectMgr->AddSpellToTrainer(ItemVendor::Entry, trainerSpell.spell, trainerSpell.spellCost,
                                      trainerSpell.reqSkill, trainerSpell.reqSkillValue, trainerSpell.reqLevel,
                                      trainerSpell.reqSpell);
        ++mergedSpells;
      }
    }

    LOG_INFO("server.loading", ">> arenacraft: merged {} class trainer spells into vendor {}", mergedSpells,
             ItemVendor::Entry);
  }
}
} // namespace arenacraft
