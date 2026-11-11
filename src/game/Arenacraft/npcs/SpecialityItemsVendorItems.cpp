#include "SpecialityItemsVendor.hpp"

// Flat, code-defined Tier 9 vendor stock: the 25-player heroic (ilvl 258)
// "Triumphant" sets for both Alliance and Horde, grouped by armor type.
// Source: https://www.wowhead.com/wotlk/guide/raids/tier-9-sets-overview
// (each set page lists its five pieces). Row order is the vendor order;
// inside a set the pieces are head, shoulders, chest, hands, legs.
//
// Other speciality stock belongs in this file too; add a group accessor and
// expose it from SpecialityItemsVendor.hpp when the vendor grows.

namespace arenacraft
{
namespace
{
std::vector<uint32> const& PlateItems()
{
  static std::vector<uint32> const items = {
      // == Alliance ==
      // Death Knight
      // Triumphant Thassarian's Plate
      48545, // Thassarian's Faceguard of Triumph
      48543, // Thassarian's Pauldrons of Triumph
      48547, // Thassarian's Chestguard of Triumph
      48546, // Thassarian's Handguards of Triumph
      48544, // Thassarian's Legguards of Triumph
      // Triumphant Thassarian's Battlegear
      48488, // Thassarian's Helmet of Triumph
      48486, // Thassarian's Shoulderplates of Triumph
      48490, // Thassarian's Battleplate of Triumph
      48489, // Thassarian's Gauntlets of Triumph
      48487, // Thassarian's Legplates of Triumph
      // Paladin
      // Triumphant Turalyon's Garb
      48582, // Turalyon's Headpiece of Triumph
      48580, // Turalyon's Spaulders of Triumph
      48584, // Turalyon's Tunic of Triumph
      48583, // Turalyon's Gloves of Triumph
      48581, // Turalyon's Greaves of Triumph
      // Triumphant Turalyon's Plate
      48644, // Turalyon's Faceguard of Triumph
      48646, // Turalyon's Shoulderguards of Triumph
      48642, // Turalyon's Breastplate of Triumph
      48643, // Turalyon's Handguards of Triumph
      48645, // Turalyon's Legguards of Triumph
      // Triumphant Turalyon's Battlegear
      48614, // Turalyon's Helm of Triumph
      48612, // Turalyon's Shoulderplates of Triumph
      48616, // Turalyon's Battleplate of Triumph
      48615, // Turalyon's Gauntlets of Triumph
      48613, // Turalyon's Legplates of Triumph
      // Warrior
      // Triumphant Wrynn's Battlegear
      48383, // Wrynn's Helmet of Triumph
      48381, // Wrynn's Shoulderplates of Triumph
      48385, // Wrynn's Battleplate of Triumph
      48384, // Wrynn's Gauntlets of Triumph
      48382, // Wrynn's Legplates of Triumph
      // Triumphant Wrynn's Plate
      48433, // Wrynn's Greathelm of Triumph
      48455, // Wrynn's Pauldrons of Triumph
      48451, // Wrynn's Breastplate of Triumph
      48453, // Wrynn's Handguards of Triumph
      48447, // Wrynn's Legguards of Triumph
      // == Horde ==
      // Death Knight
      // Triumphant Koltira's Plate
      48550, // Koltira's Faceguard of Triumph
      48552, // Koltira's Pauldrons of Triumph
      48548, // Koltira's Chestguard of Triumph
      48549, // Koltira's Handguards of Triumph
      48551, // Koltira's Legguards of Triumph
      // Triumphant Koltira's Battlegear
      48493, // Koltira's Helmet of Triumph
      48495, // Koltira's Shoulderplates of Triumph
      48491, // Koltira's Battleplate of Triumph
      48492, // Koltira's Gauntlets of Triumph
      48494, // Koltira's Legplates of Triumph
      // Paladin
      // Triumphant Liadrin's Garb
      48587, // Liadrin's Headpiece of Triumph
      48585, // Liadrin's Spaulders of Triumph
      48589, // Liadrin's Tunic of Triumph
      48588, // Liadrin's Gloves of Triumph
      48586, // Liadrin's Greaves of Triumph
      // Triumphant Liadrin's Plate
      48649, // Liadrin's Faceguard of Triumph
      48647, // Liadrin's Shoulderguards of Triumph
      48651, // Liadrin's Breastplate of Triumph
      48650, // Liadrin's Handguards of Triumph
      48648, // Liadrin's Legguards of Triumph
      // Triumphant Liadrin's Battlegear
      48619, // Liadrin's Helm of Triumph
      48621, // Liadrin's Shoulderplates of Triumph
      48617, // Liadrin's Battleplate of Triumph
      48618, // Liadrin's Gauntlets of Triumph
      48620, // Liadrin's Legplates of Triumph
      // Warrior
      // Triumphant Hellscream's Battlegear
      48398, // Hellscream's Helmet of Triumph
      48400, // Hellscream's Shoulderplates of Triumph
      48396, // Hellscream's Battleplate of Triumph
      48397, // Hellscream's Gauntlets of Triumph
      48399, // Hellscream's Legplates of Triumph
      // Triumphant Hellscream's Plate
      48468, // Hellscream's Greathelm of Triumph
      48470, // Hellscream's Pauldrons of Triumph
      48466, // Hellscream's Breastplate of Triumph
      48467, // Hellscream's Handguards of Triumph
      48469, // Hellscream's Legguards of Triumph
  };
  return items;
}

std::vector<uint32> const& MailItems()
{
  static std::vector<uint32> const items = {
      // == Alliance ==
      // Hunter
      // Triumphant Windrunner's Battlegear
      48262, // Windrunner's Headpiece of Triumph
      48260, // Windrunner's Spaulders of Triumph
      48264, // Windrunner's Tunic of Triumph
      48263, // Windrunner's Handguards of Triumph
      48261, // Windrunner's Legguards of Triumph
      // Shaman
      // Triumphant Nobundo's Regalia
      48323, // Nobundo's Helm of Triumph
      48321, // Nobundo's Shoulderpads of Triumph
      48325, // Nobundo's Hauberk of Triumph
      48324, // Nobundo's Gloves of Triumph
      48322, // Nobundo's Kilt of Triumph
      // Triumphant Nobundo's Battlegear
      48353, // Nobundo's Faceguard of Triumph
      48351, // Nobundo's Shoulderguards of Triumph
      48355, // Nobundo's Chestguard of Triumph
      48354, // Nobundo's Grips of Triumph
      48352, // Nobundo's War-Kilt of Triumph
      // Triumphant Nobundo's Garb
      48292, // Nobundo's Headpiece of Triumph
      48290, // Nobundo's Spaulders of Triumph
      48294, // Nobundo's Tunic of Triumph
      48293, // Nobundo's Handguards of Triumph
      48291, // Nobundo's Legguards of Triumph
      // == Horde ==
      // Hunter
      // Triumphant Windrunner's Pursuit
      48267, // Windrunner's Headpiece of Triumph
      48269, // Windrunner's Spaulders of Triumph
      48265, // Windrunner's Tunic of Triumph
      48266, // Windrunner's Handguards of Triumph
      48268, // Windrunner's Legguards of Triumph
      // Shaman
      // Triumphant Thrall's Regalia
      48328, // Thrall's Helm of Triumph
      48330, // Thrall's Shoulderpads of Triumph
      48326, // Thrall's Hauberk of Triumph
      48327, // Thrall's Gloves of Triumph
      48329, // Thrall's Kilt of Triumph
      // Triumphant Thrall's Battlegear
      48358, // Thrall's Faceguard of Triumph
      48360, // Thrall's Shoulderguards of Triumph
      48356, // Thrall's Chestguard of Triumph
      48357, // Thrall's Grips of Triumph
      48359, // Thrall's War-Kilt of Triumph
      // Triumphant Thrall's Garb
      48307, // Thrall's Headpiece of Triumph
      48309, // Thrall's Spaulders of Triumph
      48305, // Thrall's Tunic of Triumph
      48306, // Thrall's Handguards of Triumph
      48308, // Thrall's Legguards of Triumph
  };
  return items;
}

std::vector<uint32> const& LeatherItems()
{
  static std::vector<uint32> const items = {
      // == Alliance ==
      // Druid
      // Triumphant Malfurion's Regalia
      48171, // Malfurion's Cover of Triumph
      48168, // Malfurion's Mantle of Triumph
      48169, // Malfurion's Vestments of Triumph
      48172, // Malfurion's Gloves of Triumph
      48170, // Malfurion's Trousers of Triumph
      // Triumphant Malfurion's Battlegear
      48204, // Malfurion's Headguard of Triumph
      48207, // Malfurion's Shoulderpads of Triumph
      48206, // Malfurion's Raiments of Triumph
      48203, // Malfurion's Handgrips of Triumph
      48205, // Malfurion's Legguards of Triumph
      // Triumphant Malfurion's Garb
      48141, // Malfurion's Headpiece of Triumph
      48138, // Malfurion's Spaulders of Triumph
      48139, // Malfurion's Robe of Triumph
      48142, // Malfurion's Handguards of Triumph
      48140, // Malfurion's Leggings of Triumph
      // Rogue
      // Triumphant VanCleef's Battlegear
      48230, // VanCleef's Helmet of Triumph
      48228, // VanCleef's Pauldrons of Triumph
      48232, // VanCleef's Breastplate of Triumph
      48231, // VanCleef's Gauntlets of Triumph
      48229, // VanCleef's Legplates of Triumph
      // == Horde ==
      // Druid
      // Triumphant Runetotem's Regalia
      48174, // Runetotem's Cover of Triumph
      48177, // Runetotem's Mantle of Triumph
      48176, // Runetotem's Vestments of Triumph
      48173, // Runetotem's Gloves of Triumph
      48175, // Runetotem's Trousers of Triumph
      // Triumphant Runetotem's Battlegear
      48201, // Runetotem's Headguard of Triumph
      48198, // Runetotem's Shoulderpads of Triumph
      48199, // Runetotem's Raiments of Triumph
      48202, // Runetotem's Handgrips of Triumph
      48200, // Runetotem's Legguards of Triumph
      // Triumphant Runetotem's Garb
      48144, // Runetotem's Headpiece of Triumph
      48147, // Runetotem's Spaulders of Triumph
      48146, // Runetotem's Robe of Triumph
      48143, // Runetotem's Handguards of Triumph
      48145, // Runetotem's Leggings of Triumph
      // Rogue
      // Triumphant Garona's Battlegear
      48235, // Garona's Helmet of Triumph
      48237, // Garona's Pauldrons of Triumph
      48233, // Garona's Breastplate of Triumph
      48234, // Garona's Gauntlets of Triumph
      48236, // Garona's Legplates of Triumph
  };
  return items;
}

std::vector<uint32> const& ClothItems()
{
  static std::vector<uint32> const items = {
      // == Alliance ==
      // Mage
      // Triumphant Khadgar's Regalia
      47761, // Khadgar's Hood of Triumph
      47758, // Khadgar's Shoulderpads of Triumph
      47759, // Khadgar's Robe of Triumph
      47762, // Khadgar's Gauntlets of Triumph
      47760, // Khadgar's Leggings of Triumph
      // Priest
      // Triumphant Velen's Raiment
      48035, // Velen's Cowl of Triumph
      48029, // Velen's Shoulderpads of Triumph
      48031, // Velen's Robe of Triumph
      48037, // Velen's Gloves of Triumph
      48033, // Velen's Leggings of Triumph
      // Triumphant Velen's Regalia
      48085, // Velen's Circlet of Triumph
      48082, // Velen's Mantle of Triumph
      48083, // Velen's Raiments of Triumph
      48086, // Velen's Handwraps of Triumph
      48084, // Velen's Pants of Triumph
      // Warlock
      // Triumphant Kel'Thuzad's Regalia
      47789, // Kel'Thuzad's Hood of Triumph
      47792, // Kel'Thuzad's Shoulderpads of Triumph
      47791, // Kel'Thuzad's Robe of Triumph
      47788, // Kel'Thuzad's Gloves of Triumph
      47790, // Kel'Thuzad's Leggings of Triumph
      // == Horde ==
      // Mage
      // Triumphant Sunstrider's Regalia
      47764, // Sunstrider's Hood of Triumph
      47767, // Sunstrider's Shoulderpads of Triumph
      47766, // Sunstrider's Robe of Triumph
      47763, // Sunstrider's Gauntlets of Triumph
      47765, // Sunstrider's Leggings of Triumph
      // Priest
      // Triumphant Zabra's Raiment
      48058, // Zabra's Cowl of Triumph
      48061, // Zabra's Shoulderpads of Triumph
      48060, // Zabra's Robe of Triumph
      48057, // Zabra's Gloves of Triumph
      48059, // Zabra's Leggings of Triumph
      // Triumphant Zabra's Regalia
      48088, // Zabra's Circlet of Triumph
      48091, // Zabra's Mantle of Triumph
      48090, // Zabra's Raiments of Triumph
      48087, // Zabra's Handwraps of Triumph
      48089, // Zabra's Pants of Triumph
      // Warlock
      // Triumphant Gul'dan's Regalia
      47796, // Gul'dan's Hood of Triumph
      47793, // Gul'dan's Shoulderpads of Triumph
      47794, // Gul'dan's Robe of Triumph
      47797, // Gul'dan's Gloves of Triumph
      47795, // Gul'dan's Leggings of Triumph
  };
  return items;
}

} // namespace

std::vector<Tier9Group> const& Tier9Groups()
{
  static std::vector<Tier9Group> const groups = {
      {"Plate", PlateItems()},
      {"Mail", MailItems()},
      {"Leather", LeatherItems()},
      {"Cloth", ClothItems()},
  };
  return groups;
}
} // namespace arenacraft
