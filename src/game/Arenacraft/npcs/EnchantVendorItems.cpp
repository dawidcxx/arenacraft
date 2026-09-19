#include "ItemVendor.hpp"

// Flat, code-defined enchant vendor stock: one row per permanent-enhancement
// item, ordered by gear slot (head first, weapon last). The single
// "Enchantments" gossip option serves this whole list (see ItemVendor.hpp /
// ItemVendor.cpp).
//
// Source: the Wowhead WotLK per-spec PvP Arena Season 8 BiS guides (primary),
// plus the per-spec PvE "Enchants and Gems" guides for the remaining general
// alternatives. Both are documented in ai-doc/enchant_guides.md. Only enchants
// those guides recommend are listed; vanilla/BC-era filler, strictly-dominated
// budget duplicates and everything that needs a profession to apply (the server
// has none) is dropped. The Titanium Weapon Chain is added by hand: it is
// applied directly to the weapon, so it needs no profession.
// Head/shoulder/leg entries are the applied Arcanum/Inscription/Spellthread/Leg
// Armor items; the waist and weapon sections also hold the applied Eternal Belt
// Buckle and Titanium Weapon Chain; the rest are "Scroll of Enchant ..."
// consumables.
//
// Vendor order follows row order, so moving a row up moves it up in the vendor.

namespace arenacraft
{
std::vector<uint32> const& AllEnchants()
{
  static std::vector<uint32> const enchants = {
      // == Head ==
      44149, // Arcanum of Torment
      44159, // Arcanum of Burning Mysteries
      44152, // Arcanum of Blissful Mending
      44150, // Arcanum of the Stalwart Protector
      44701, // Arcanum of the Savage Gladiator
      44075, // Arcanum of Dominance
      44069, // Arcanum of Triumph
      29192, // Arcanum of Ferocity
      // == Shoulder ==
      44133, // Greater Inscription of the Axe
      44135, // Greater Inscription of the Storm
      44134, // Greater Inscription of the Crag
      44136, // Greater Inscription of the Pinnacle
      44957, // Greater Inscription of the Gladiator
      44068, // Inscription of Dominance
      44067, // Inscription of Triumph
      // == Back ==
      44457, // Scroll of Enchant Cloak - Major Agility
      39003, // Scroll of Enchant Cloak - Greater Speed
      39001, // Scroll of Enchant Cloak - Mighty Armor
      38978, // Scroll of Enchant Cloak - Titanweave
      38973, // Scroll of Enchant Cloak - Spell Piercing
      38993, // Scroll of Enchant Cloak - Shadow Armor
      // == Chest ==
      44465, // Scroll of Enchant Chest - Powerful Stats
      38975, // Scroll of Enchant Chest - Exceptional Resilience
      39005, // Scroll of Enchant Chest - Super Health
      39002, // Scroll of Enchant Chest - Greater Defense
      38928, // Scroll of Enchant Chest - Major Spirit
      38962, // Scroll of Enchant Chest - Greater Mana Restoration
      // == Wrist ==
      44470, // Scroll of Enchant Bracer - Superior Spellpower
      44815, // Scroll of Enchant Bracers - Greater Assault
      44947, // Scroll of Enchant Bracer - Major Stamina
      38984, // Scroll of Enchant Bracer - Expertise
      38968, // Scroll of Enchant Bracers - Exceptional Intellect
      // == Hands ==
      44458, // Scroll of Enchant Gloves - Crusher
      38979, // Scroll of Enchant Gloves - Exceptional Spellpower
      38967, // Scroll of Enchant Gloves - Major Agility
      38951, // Scroll of Enchant Gloves - Expertise
      38990, // Scroll of Enchant Gloves - Armsman
      38953, // Scroll of Enchant Gloves - Precision
      38885, // Scroll of Enchant Gloves - Threat
      // == Waist ==
      41611, // Eternal Belt Buckle
      // == Legs ==
      38374, // Icescale Leg Armor
      41602, // Brilliant Spellthread
      41604, // Sapphire Spellthread
      38373, // Frosthide Leg Armor
      44963, // Earthen Leg Armor
      // == Feet ==
      38986, // Scroll of Enchant Boots - Icewalker
      39006, // Scroll of Enchant Boots - Tuskarr's Vitality
      38943, // Scroll of Enchant Boots - Cat's Swiftness
      44469, // Scroll of Enchant Boots - Greater Assault
      38966, // Scroll of Enchant Boots - Greater Fortitude
      38976, // Scroll of Enchant Boots - Superior Agility
      38961, // Scroll of Enchant Boots - Greater Spirit
      // == Shield ==
      44455, // Scroll of Enchant Shield - Greater Intellect
      38945, // Scroll of Enchant Shield - Major Stamina
      38949, // Scroll of Enchant Shield - Resilience
      38954, // Scroll of Enchant Shield - Defense
      // == Weapon ==
      44493, // Scroll of Enchant Weapon - Berserking
      44467, // Scroll of Enchant Weapon - Mighty Spellpower
      43987, // Scroll of Enchant Weapon - Black Magic
      45056, // Scroll of Enchant Staff - Greater Spellpower
      44463, // Scroll of Enchant 2H Weapon - Massacre
      44466, // Scroll of Enchant Weapon - Superior Potency
      44497, // Scroll of Enchant Weapon - Accuracy
      38925, // Scroll of Enchant Weapon - Mongoose
      46098, // Scroll of Enchant Weapon - Blood Draining
      38918, // Scroll of Enchant Weapon - Major Intellect
      41976, // Titanium Weapon Chain
  };
  return enchants;
}
} // namespace arenacraft
