#include "ItemVendor.hpp"

// Flat, code-defined "General goods" vendor stock: one row per everyday
// consumable/reagent a level-80 character needs while gearing and playing
// (conjured mage food, rogue poisons, ammo, class reagents). The single
// "General goods" gossip option serves this whole list and is the first menu
// entry.
//
// Item ids were checked on wotlk.evowow.com (and against the world DB for the
// stack/price columns). Some entries are flagged "Not available to players" on
// evowow because they are conjured (Star's Sorrow, Conjured Mana Strudel); they
// are intentionally stocked here anyway.
//
// Vendor order follows row order, so moving a row up moves it up in the vendor.

namespace arenacraft
{
std::vector<uint32> const& AllGeneralGoods()
{
  static std::vector<uint32> const items = {
      43236, // Star's Sorrow
      43523, // Conjured Mana Strudel
      6265,  // Soul Shard
      52021, // Iceblade Arrow
      52020, // Shatter Rounds
      3775,  // Crippling Poison
      5237,  // Mind-numbing Poison
      43235, // Wound Poison VII
      43231, // Instant Poison IX
      43233, // Deadly Poison IX
      43237, // Anesthetic Poison II
      17020, // Arcane Powder
      17033, // Symbol of Divinity
      17030, // Ankh
      17056, // Light Feather
      44605, // Wild Spineleaf
      44614, // Starleaf Seed
      44615, // Devout Candle
      5565,  // Infernal Stone
      21177, // Symbol of Kings
      34722, // Heavy Frostweave Bandage
  };
  return items;
}
} // namespace arenacraft
