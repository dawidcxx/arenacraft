#include "ItemVendor.hpp"

// Flat, code-defined vendor stock. Source of truth for the arenacraft
// multi-vendor (see ItemVendor.hpp / ItemVendor.cpp).
//
// To stock an item: append `{Category, <itemId>},` below with a trailing
// comment naming it. To add a vendor list: add its name to CategoryOrder()
// (menu entries follow that order) and tag rows with it.
//
// Item ids are checked against wotlk.evowow.com; anything flagged
// "Not available to players" is deliberately left out.

namespace arenacraft
{
namespace
{
constexpr std::string_view WrathfulSetWeapons = "Wrathful Set & Weapons";
constexpr std::string_view WrathfulOffparts   = "Wrathful Offparts";
constexpr std::string_view Trinkets           = "Trinkets";
constexpr std::string_view ICCSetWeapons      = "ICC Set & Weapons";
constexpr std::string_view ICCOffparts        = "ICC Offparts";
constexpr std::string_view ICCOffset          = "ICC Offset";
} // namespace

std::vector<std::string_view> const& CategoryOrder()
{
  static std::vector<std::string_view> const order = {
      WrathfulSetWeapons, WrathfulOffparts, Trinkets, ICCSetWeapons, ICCOffparts, ICCOffset,
  };
  return order;
}

std::vector<ItemEntry> const& AllItems()
{
  static std::vector<ItemEntry> const items = {
      // == Wrathful Gladiator: season 8 set (ilvl 270) ==
      // Head
      {WrathfulSetWeapons, 51460}, // Wrathful Gladiator's Chain Helm
      {WrathfulSetWeapons, 51427}, // Wrathful Gladiator's Dragonhide Helm
      {WrathfulSetWeapons, 51415}, // Wrathful Gladiator's Dreadplate Helm
      {WrathfulSetWeapons, 51538}, // Wrathful Gladiator's Felweave Cowl
      {WrathfulSetWeapons, 51421}, // Wrathful Gladiator's Kodohide Helm
      {WrathfulSetWeapons, 51494}, // Wrathful Gladiator's Leather Helm
      {WrathfulSetWeapons, 51505}, // Wrathful Gladiator's Linked Helm
      {WrathfulSetWeapons, 51511}, // Wrathful Gladiator's Mail Helm
      {WrathfulSetWeapons, 51484}, // Wrathful Gladiator's Mooncloth Hood
      {WrathfulSetWeapons, 51470}, // Wrathful Gladiator's Ornamented Headcover
      {WrathfulSetWeapons, 51543}, // Wrathful Gladiator's Plate Helm
      {WrathfulSetWeapons, 51499}, // Wrathful Gladiator's Ringmail Helm
      {WrathfulSetWeapons, 51489}, // Wrathful Gladiator's Satin Hood
      {WrathfulSetWeapons, 51476}, // Wrathful Gladiator's Scaled Helm
      {WrathfulSetWeapons, 51465}, // Wrathful Gladiator's Silk Cowl
      {WrathfulSetWeapons, 51435}, // Wrathful Gladiator's Wyrmhide Helm
      // Shoulders
      {WrathfulSetWeapons, 51462}, // Wrathful Gladiator's Chain Spaulders
      {WrathfulSetWeapons, 51430}, // Wrathful Gladiator's Dragonhide Spaulders
      {WrathfulSetWeapons, 51418}, // Wrathful Gladiator's Dreadplate Shoulders
      {WrathfulSetWeapons, 51540}, // Wrathful Gladiator's Felweave Amice
      {WrathfulSetWeapons, 51424}, // Wrathful Gladiator's Kodohide Spaulders
      {WrathfulSetWeapons, 51496}, // Wrathful Gladiator's Leather Spaulders
      {WrathfulSetWeapons, 51508}, // Wrathful Gladiator's Linked Spaulders
      {WrathfulSetWeapons, 51514}, // Wrathful Gladiator's Mail Spaulders
      {WrathfulSetWeapons, 51486}, // Wrathful Gladiator's Mooncloth Mantle
      {WrathfulSetWeapons, 51473}, // Wrathful Gladiator's Ornamented Spaulders
      {WrathfulSetWeapons, 51545}, // Wrathful Gladiator's Plate Shoulders
      {WrathfulSetWeapons, 51502}, // Wrathful Gladiator's Ringmail Spaulders
      {WrathfulSetWeapons, 51491}, // Wrathful Gladiator's Satin Mantle
      {WrathfulSetWeapons, 51479}, // Wrathful Gladiator's Scaled Shoulders
      {WrathfulSetWeapons, 51467}, // Wrathful Gladiator's Silk Amice
      {WrathfulSetWeapons, 51438}, // Wrathful Gladiator's Wyrmhide Spaulders
      // Chest
      {WrathfulSetWeapons, 51458}, // Wrathful Gladiator's Chain Armor
      {WrathfulSetWeapons, 51425}, // Wrathful Gladiator's Dragonhide Robes
      {WrathfulSetWeapons, 51413}, // Wrathful Gladiator's Dreadplate Chestpiece
      {WrathfulSetWeapons, 51536}, // Wrathful Gladiator's Felweave Raiment
      {WrathfulSetWeapons, 51419}, // Wrathful Gladiator's Kodohide Robes
      {WrathfulSetWeapons, 51492}, // Wrathful Gladiator's Leather Tunic
      {WrathfulSetWeapons, 51503}, // Wrathful Gladiator's Linked Armor
      {WrathfulSetWeapons, 51509}, // Wrathful Gladiator's Mail Armor
      {WrathfulSetWeapons, 51482}, // Wrathful Gladiator's Mooncloth Robe
      {WrathfulSetWeapons, 51468}, // Wrathful Gladiator's Ornamented Chestguard
      {WrathfulSetWeapons, 51541}, // Wrathful Gladiator's Plate Chestpiece
      {WrathfulSetWeapons, 51497}, // Wrathful Gladiator's Ringmail Armor
      {WrathfulSetWeapons, 51487}, // Wrathful Gladiator's Satin Robe
      {WrathfulSetWeapons, 51474}, // Wrathful Gladiator's Scaled Chestpiece
      {WrathfulSetWeapons, 51463}, // Wrathful Gladiator's Silk Raiment
      {WrathfulSetWeapons, 51433}, // Wrathful Gladiator's Wyrmhide Robes
      // Legs
      {WrathfulSetWeapons, 51461}, // Wrathful Gladiator's Chain Leggings
      {WrathfulSetWeapons, 51428}, // Wrathful Gladiator's Dragonhide Legguards
      {WrathfulSetWeapons, 51416}, // Wrathful Gladiator's Dreadplate Legguards
      {WrathfulSetWeapons, 51539}, // Wrathful Gladiator's Felweave Trousers
      {WrathfulSetWeapons, 51422}, // Wrathful Gladiator's Kodohide Legguards
      {WrathfulSetWeapons, 51495}, // Wrathful Gladiator's Leather Legguards
      {WrathfulSetWeapons, 51506}, // Wrathful Gladiator's Linked Leggings
      {WrathfulSetWeapons, 51512}, // Wrathful Gladiator's Mail Leggings
      {WrathfulSetWeapons, 51485}, // Wrathful Gladiator's Mooncloth Leggings
      {WrathfulSetWeapons, 51471}, // Wrathful Gladiator's Ornamented Legplates
      {WrathfulSetWeapons, 51544}, // Wrathful Gladiator's Plate Legguards
      {WrathfulSetWeapons, 51500}, // Wrathful Gladiator's Ringmail Leggings
      {WrathfulSetWeapons, 51490}, // Wrathful Gladiator's Satin Leggings
      {WrathfulSetWeapons, 51477}, // Wrathful Gladiator's Scaled Legguards
      {WrathfulSetWeapons, 51466}, // Wrathful Gladiator's Silk Trousers
      {WrathfulSetWeapons, 51436}, // Wrathful Gladiator's Wyrmhide Legguards
      // Hands
      {WrathfulSetWeapons, 51459}, // Wrathful Gladiator's Chain Gauntlets
      {WrathfulSetWeapons, 51426}, // Wrathful Gladiator's Dragonhide Gloves
      {WrathfulSetWeapons, 51414}, // Wrathful Gladiator's Dreadplate Gauntlets
      {WrathfulSetWeapons, 51537}, // Wrathful Gladiator's Felweave Handguards
      {WrathfulSetWeapons, 51420}, // Wrathful Gladiator's Kodohide Gloves
      {WrathfulSetWeapons, 51493}, // Wrathful Gladiator's Leather Gloves
      {WrathfulSetWeapons, 51504}, // Wrathful Gladiator's Linked Gauntlets
      {WrathfulSetWeapons, 51510}, // Wrathful Gladiator's Mail Gauntlets
      {WrathfulSetWeapons, 51483}, // Wrathful Gladiator's Mooncloth Gloves
      {WrathfulSetWeapons, 51469}, // Wrathful Gladiator's Ornamented Gloves
      {WrathfulSetWeapons, 51542}, // Wrathful Gladiator's Plate Gauntlets
      {WrathfulSetWeapons, 51498}, // Wrathful Gladiator's Ringmail Gauntlets
      {WrathfulSetWeapons, 51488}, // Wrathful Gladiator's Satin Gloves
      {WrathfulSetWeapons, 51475}, // Wrathful Gladiator's Scaled Gauntlets
      {WrathfulSetWeapons, 51464}, // Wrathful Gladiator's Silk Handguards
      {WrathfulSetWeapons, 51434}, // Wrathful Gladiator's Wyrmhide Gloves

      // == Wrathful Gladiator: weapons (ilvl 277) ==
      // One-Hand
      {WrathfulSetWeapons, 51516}, // Wrathful Gladiator's Handaxe
      {WrathfulSetWeapons, 51522}, // Wrathful Gladiator's Longblade
      {WrathfulSetWeapons, 51518}, // Wrathful Gladiator's Spike
      {WrathfulSetWeapons, 51520}, // Wrathful Gladiator's Truncheon
      // Ranged
      {WrathfulSetWeapons, 51395}, // Wrathful Gladiator's Recurve
      {WrathfulSetWeapons, 51412}, // Wrathful Gladiator's Repeater
      {WrathfulSetWeapons, 51450}, // Wrathful Gladiator's Shotgun
      // Two-Hand
      {WrathfulSetWeapons, 51403}, // Wrathful Gladiator's Acute Staff
      {WrathfulSetWeapons, 51393}, // Wrathful Gladiator's Claymore
      {WrathfulSetWeapons, 51401}, // Wrathful Gladiator's Combat Staff
      {WrathfulSetWeapons, 51391}, // Wrathful Gladiator's Crusher
      {WrathfulSetWeapons, 51432}, // Wrathful Gladiator's Greatstaff
      {WrathfulSetWeapons, 51481}, // Wrathful Gladiator's Halberd
      {WrathfulSetWeapons, 51457}, // Wrathful Gladiator's Light Staff
      {WrathfulSetWeapons, 51405}, // Wrathful Gladiator's Skirmish Staff
      {WrathfulSetWeapons, 51389}, // Wrathful Gladiator's Sunderer
      // Main Hand
      {WrathfulSetWeapons, 51398}, // Wrathful Gladiator's Blade of Celerity
      {WrathfulSetWeapons, 51524}, // Wrathful Gladiator's Grasp
      {WrathfulSetWeapons, 51399}, // Wrathful Gladiator's Mageblade
      {WrathfulSetWeapons, 51454}, // Wrathful Gladiator's Salvation
      // Off Hand
      {WrathfulSetWeapons, 51440}, // Wrathful Gladiator's Dicer
      {WrathfulSetWeapons, 51442}, // Wrathful Gladiator's Dirk
      {WrathfulSetWeapons, 51528}, // Wrathful Gladiator's Eviscerator
      {WrathfulSetWeapons, 51529}, // Wrathful Gladiator's Left Claw
      {WrathfulSetWeapons, 51444}, // Wrathful Gladiator's Left Razor
      {WrathfulSetWeapons, 51446}, // Wrathful Gladiator's Punisher
      {WrathfulSetWeapons, 51526}, // Wrathful Gladiator's Splitter
      {WrathfulSetWeapons, 51448}, // Wrathful Gladiator's Swiftblade

      // == Wrathful Gladiator: shields / off-hands / relics (ilvl 270) ==
      // Shield
      {WrathfulSetWeapons, 51452}, // Wrathful Gladiator's Barrier
      {WrathfulSetWeapons, 51455}, // Wrathful Gladiator's Redoubt
      {WrathfulSetWeapons, 51533}, // Wrathful Gladiator's Shield Wall
      // Held In Off-Hand
      {WrathfulSetWeapons, 51407}, // Wrathful Gladiator's Compendium
      {WrathfulSetWeapons, 51396}, // Wrathful Gladiator's Endgame
      {WrathfulSetWeapons, 51408}, // Wrathful Gladiator's Grimoire
      {WrathfulSetWeapons, 51409}, // Wrathful Gladiator's Reprieve
      // Relic
      {WrathfulSetWeapons, 51429}, // Wrathful Gladiator's Idol of Resolve
      {WrathfulSetWeapons, 51437}, // Wrathful Gladiator's Idol of Steadfastness
      {WrathfulSetWeapons, 51423}, // Wrathful Gladiator's Idol of Tenacity
      {WrathfulSetWeapons, 51478}, // Wrathful Gladiator's Libram of Fortitude
      {WrathfulSetWeapons, 51472}, // Wrathful Gladiator's Libram of Justice
      {WrathfulSetWeapons, 51417}, // Wrathful Gladiator's Sigil of Strife
      {WrathfulSetWeapons, 51507}, // Wrathful Gladiator's Totem of Indomitability
      {WrathfulSetWeapons, 51513}, // Wrathful Gladiator's Totem of Survival
      {WrathfulSetWeapons, 51501}, // Wrathful Gladiator's Totem of the Third Wind

      // == Wrathful Gladiator offparts (season 8, ilvl 264) ==
      // Belts
      {WrathfulOffparts, 51327}, // Wrathful Gladiator's Cord of Dominance (cloth)
      {WrathfulOffparts, 51337}, // Wrathful Gladiator's Cord of Alacrity (cloth)
      {WrathfulOffparts, 51365}, // Wrathful Gladiator's Cord of Salvation (cloth)
      {WrathfulOffparts, 51340}, // Wrathful Gladiator's Belt of Salvation (leather)
      {WrathfulOffparts, 51343}, // Wrathful Gladiator's Belt of Dominance (leather)
      {WrathfulOffparts, 51368}, // Wrathful Gladiator's Belt of Triumph (leather)
      {WrathfulOffparts, 51350}, // Wrathful Gladiator's Waistguard of Triumph (mail)
      {WrathfulOffparts, 51371}, // Wrathful Gladiator's Waistguard of Salvation (mail)
      {WrathfulOffparts, 51374}, // Wrathful Gladiator's Waistguard of Dominance (mail)
      {WrathfulOffparts, 51359}, // Wrathful Gladiator's Girdle of Salvation (plate)
      {WrathfulOffparts, 51362}, // Wrathful Gladiator's Girdle of Triumph (plate)
      // Feet
      {WrathfulOffparts, 51328}, // Wrathful Gladiator's Treads of Dominance (cloth)
      {WrathfulOffparts, 51338}, // Wrathful Gladiator's Treads of Alacrity (cloth)
      {WrathfulOffparts, 51366}, // Wrathful Gladiator's Treads of Salvation (cloth)
      {WrathfulOffparts, 51341}, // Wrathful Gladiator's Boots of Salvation (leather)
      {WrathfulOffparts, 51344}, // Wrathful Gladiator's Boots of Dominance (leather)
      {WrathfulOffparts, 51369}, // Wrathful Gladiator's Boots of Triumph (leather)
      {WrathfulOffparts, 51351}, // Wrathful Gladiator's Sabatons of Triumph (mail)
      {WrathfulOffparts, 51372}, // Wrathful Gladiator's Sabatons of Salvation (mail)
      {WrathfulOffparts, 51375}, // Wrathful Gladiator's Sabatons of Dominance (mail)
      {WrathfulOffparts, 51360}, // Wrathful Gladiator's Greaves of Salvation (plate)
      {WrathfulOffparts, 51363}, // Wrathful Gladiator's Greaves of Triumph (plate)
      // Wrists
      {WrathfulOffparts, 51329}, // Wrathful Gladiator's Cuffs of Dominance (cloth)
      {WrathfulOffparts, 51339}, // Wrathful Gladiator's Cuffs of Alacrity (cloth)
      {WrathfulOffparts, 51367}, // Wrathful Gladiator's Cuffs of Salvation (cloth)
      {WrathfulOffparts, 51342}, // Wrathful Gladiator's Armwraps of Salvation (leather)
      {WrathfulOffparts, 51345}, // Wrathful Gladiator's Armwraps of Dominance (leather)
      {WrathfulOffparts, 51370}, // Wrathful Gladiator's Armwraps of Triumph (leather)
      {WrathfulOffparts, 51352}, // Wrathful Gladiator's Wristguards of Triumph (mail)
      {WrathfulOffparts, 51373}, // Wrathful Gladiator's Wristguards of Salvation (mail)
      {WrathfulOffparts, 51376}, // Wrathful Gladiator's Wristguards of Dominance (mail)
      {WrathfulOffparts, 51361}, // Wrathful Gladiator's Bracers of Salvation (plate)
      {WrathfulOffparts, 51364}, // Wrathful Gladiator's Bracers of Triumph (plate)
      // Rings
      {WrathfulOffparts, 51336}, // Wrathful Gladiator's Band of Dominance (spell)
      {WrathfulOffparts, 51358}, // Wrathful Gladiator's Band of Triumph (melee)
      {WrathfulOffparts, 42118}, // Relentless Gladiator's Band of Ascendancy
      {WrathfulOffparts, 42119}, // Relentless Gladiator's Band of Victory
      // Neck
      {WrathfulOffparts, 51331}, // Wrathful Gladiator's Pendant of Dominance
      {WrathfulOffparts, 51333}, // Wrathful Gladiator's Pendant of Subjugation
      {WrathfulOffparts, 51335}, // Wrathful Gladiator's Pendant of Ascendancy
      {WrathfulOffparts, 51347}, // Wrathful Gladiator's Pendant of Salvation
      {WrathfulOffparts, 51349}, // Wrathful Gladiator's Pendant of Deliverance
      {WrathfulOffparts, 51353}, // Wrathful Gladiator's Pendant of Sundering
      {WrathfulOffparts, 51355}, // Wrathful Gladiator's Pendant of Triumph
      {WrathfulOffparts, 51357}, // Wrathful Gladiator's Pendant of Victory
      // Back
      {WrathfulOffparts, 51330}, // Wrathful Gladiator's Cloak of Dominance
      {WrathfulOffparts, 51332}, // Wrathful Gladiator's Cloak of Subjugation
      {WrathfulOffparts, 51334}, // Wrathful Gladiator's Cloak of Ascendancy
      {WrathfulOffparts, 51346}, // Wrathful Gladiator's Cloak of Salvation
      {WrathfulOffparts, 51348}, // Wrathful Gladiator's Cloak of Deliverance
      {WrathfulOffparts, 51354}, // Wrathful Gladiator's Cloak of Triumph
      {WrathfulOffparts, 51356}, // Wrathful Gladiator's Cloak of Victory

      // == Trinkets ==
      // ICC, ilvl 264
      {Trinkets, 50343}, // Whispering Fanged Skull
      {Trinkets, 50344}, // Unidentifiable Organ
      {Trinkets, 50345}, // Muradin's Spyglass
      {Trinkets, 50346}, // Sliver of Pure Ice
      {Trinkets, 50351}, // Tiny Abomination in a Jar
      {Trinkets, 50352}, // Corpse Tongue Coin
      {Trinkets, 50353}, // Dislodged Foreign Object
      {Trinkets, 50354}, // Bauble of True Blood
      {Trinkets, 50355}, // Herkuml War Token
      {Trinkets, 50356}, // Corroded Skeleton Key
      {Trinkets, 50357}, // Maghia's Misguided Quill
      {Trinkets, 50358}, // Purified Lunar Dust
      {Trinkets, 50359}, // Althor's Abacus
      {Trinkets, 50360}, // Phylactery of the Nameless Lich
      {Trinkets, 50361}, // Sindragosa's Flawless Fang
      {Trinkets, 50362}, // Deathbringer's Will
      {Trinkets, 51377}, // Medallion of the Alliance (PvP)
      {Trinkets, 51378}, // Medallion of the Horde (PvP)
      // Trial of the Grand Crusader 25, ilvl 258
      {Trinkets, 47059}, // Solace of the Defeated
      {Trinkets, 47088}, // Satrina's Impeding Scarab
      {Trinkets, 47131}, // Death's Verdict
      {Trinkets, 47188}, // Reign of the Unliving
      {Trinkets, 47432}, // Solace of the Fallen
      {Trinkets, 47451}, // Juggernaut's Vitality
      {Trinkets, 47464}, // Death's Choice
      {Trinkets, 47477}, // Reign of the Dead
      // Trial of the Crusader 25, ilvl 245 (normal)
      {Trinkets, 47041}, // Solace of the Defeated
      {Trinkets, 47080}, // Satrina's Impeding Scarab
      {Trinkets, 47115}, // Death's Verdict
      {Trinkets, 47182}, // Reign of the Unliving
      {Trinkets, 47271}, // Solace of the Fallen
      {Trinkets, 47290}, // Juggernaut's Vitality
      {Trinkets, 47303}, // Death's Choice
      {Trinkets, 47316}, // Reign of the Dead

      // == ICC tier 10 "Sanctified" set (ilvl 264) ==
      // Head
      {ICCSetWeapons, 51153}, // Sanctified Ahn'Kahar Blood Hunter's Headpiece
      {ICCSetWeapons, 51158}, // Sanctified Bloodmage Hood
      {ICCSetWeapons, 51184}, // Sanctified Crimson Acolyte Cowl
      {ICCSetWeapons, 51178}, // Sanctified Crimson Acolyte Hood
      {ICCSetWeapons, 51208}, // Sanctified Dark Coven Hood
      {ICCSetWeapons, 51197}, // Sanctified Frost Witch's Faceguard
      {ICCSetWeapons, 51192}, // Sanctified Frost Witch's Headpiece
      {ICCSetWeapons, 51202}, // Sanctified Frost Witch's Helm
      {ICCSetWeapons, 51149}, // Sanctified Lasherweave Cover
      {ICCSetWeapons, 51143}, // Sanctified Lasherweave Headguard
      {ICCSetWeapons, 51137}, // Sanctified Lasherweave Helmet
      {ICCSetWeapons, 51173}, // Sanctified Lightsworn Faceguard
      {ICCSetWeapons, 51167}, // Sanctified Lightsworn Headpiece
      {ICCSetWeapons, 51162}, // Sanctified Lightsworn Helmet
      {ICCSetWeapons, 51133}, // Sanctified Scourgelord Faceguard
      {ICCSetWeapons, 51127}, // Sanctified Scourgelord Helmet
      {ICCSetWeapons, 51187}, // Sanctified Shadowblade Helmet
      {ICCSetWeapons, 51218}, // Sanctified Ymirjar Lord's Greathelm
      {ICCSetWeapons, 51212}, // Sanctified Ymirjar Lord's Helmet
      // Shoulders
      {ICCSetWeapons, 51151}, // Sanctified Ahn'Kahar Blood Hunter's Spaulders
      {ICCSetWeapons, 51155}, // Sanctified Bloodmage Shoulderpads
      {ICCSetWeapons, 51182}, // Sanctified Crimson Acolyte Mantle
      {ICCSetWeapons, 51175}, // Sanctified Crimson Acolyte Shoulderpads
      {ICCSetWeapons, 51205}, // Sanctified Dark Coven Shoulderpads
      {ICCSetWeapons, 51199}, // Sanctified Frost Witch's Shoulderguards
      {ICCSetWeapons, 51204}, // Sanctified Frost Witch's Shoulderpads
      {ICCSetWeapons, 51194}, // Sanctified Frost Witch's Spaulders
      {ICCSetWeapons, 51147}, // Sanctified Lasherweave Mantle
      {ICCSetWeapons, 51135}, // Sanctified Lasherweave Pauldrons
      {ICCSetWeapons, 51140}, // Sanctified Lasherweave Shoulderpads
      {ICCSetWeapons, 51170}, // Sanctified Lightsworn Shoulderguards
      {ICCSetWeapons, 51160}, // Sanctified Lightsworn Shoulderplates
      {ICCSetWeapons, 51166}, // Sanctified Lightsworn Spaulders
      {ICCSetWeapons, 51130}, // Sanctified Scourgelord Pauldrons
      {ICCSetWeapons, 51125}, // Sanctified Scourgelord Shoulderplates
      {ICCSetWeapons, 51185}, // Sanctified Shadowblade Pauldrons
      {ICCSetWeapons, 51215}, // Sanctified Ymirjar Lord's Pauldrons
      {ICCSetWeapons, 51210}, // Sanctified Ymirjar Lord's Shoulderplates
      // Chest
      {ICCSetWeapons, 51150}, // Sanctified Ahn'Kahar Blood Hunter's Tunic
      {ICCSetWeapons, 51156}, // Sanctified Bloodmage Robe
      {ICCSetWeapons, 51180}, // Sanctified Crimson Acolyte Raiments
      {ICCSetWeapons, 51176}, // Sanctified Crimson Acolyte Robe
      {ICCSetWeapons, 51206}, // Sanctified Dark Coven Robe
      {ICCSetWeapons, 51195}, // Sanctified Frost Witch's Chestguard
      {ICCSetWeapons, 51200}, // Sanctified Frost Witch's Hauberk
      {ICCSetWeapons, 51190}, // Sanctified Frost Witch's Tunic
      {ICCSetWeapons, 51141}, // Sanctified Lasherweave Raiment
      {ICCSetWeapons, 51139}, // Sanctified Lasherweave Robes
      {ICCSetWeapons, 51145}, // Sanctified Lasherweave Vestment
      {ICCSetWeapons, 51164}, // Sanctified Lightsworn Battleplate
      {ICCSetWeapons, 51174}, // Sanctified Lightsworn Chestguard
      {ICCSetWeapons, 51165}, // Sanctified Lightsworn Tunic
      {ICCSetWeapons, 51129}, // Sanctified Scourgelord Battleplate
      {ICCSetWeapons, 51134}, // Sanctified Scourgelord Chestguard
      {ICCSetWeapons, 51189}, // Sanctified Shadowblade Breastplate
      {ICCSetWeapons, 51214}, // Sanctified Ymirjar Lord's Battleplate
      {ICCSetWeapons, 51219}, // Sanctified Ymirjar Lord's Breastplate
      // Legs
      {ICCSetWeapons, 51152}, // Sanctified Ahn'Kahar Blood Hunter's Legguards
      {ICCSetWeapons, 51157}, // Sanctified Bloodmage Leggings
      {ICCSetWeapons, 51177}, // Sanctified Crimson Acolyte Leggings
      {ICCSetWeapons, 51181}, // Sanctified Crimson Acolyte Pants
      {ICCSetWeapons, 51207}, // Sanctified Dark Coven Leggings
      {ICCSetWeapons, 51203}, // Sanctified Frost Witch's Kilt
      {ICCSetWeapons, 51193}, // Sanctified Frost Witch's Legguards
      {ICCSetWeapons, 51198}, // Sanctified Frost Witch's War-Kilt
      {ICCSetWeapons, 51142}, // Sanctified Lasherweave Legguards
      {ICCSetWeapons, 51136}, // Sanctified Lasherweave Legplates
      {ICCSetWeapons, 51146}, // Sanctified Lasherweave Trousers
      {ICCSetWeapons, 51168}, // Sanctified Lightsworn Greaves
      {ICCSetWeapons, 51171}, // Sanctified Lightsworn Legguards
      {ICCSetWeapons, 51161}, // Sanctified Lightsworn Legplates
      {ICCSetWeapons, 51131}, // Sanctified Scourgelord Legguards
      {ICCSetWeapons, 51126}, // Sanctified Scourgelord Legplates
      {ICCSetWeapons, 51186}, // Sanctified Shadowblade Legplates
      {ICCSetWeapons, 51216}, // Sanctified Ymirjar Lord's Legguards
      {ICCSetWeapons, 51211}, // Sanctified Ymirjar Lord's Legplates
      // Hands
      {ICCSetWeapons, 51154}, // Sanctified Ahn'Kahar Blood Hunter's Handguards
      {ICCSetWeapons, 51159}, // Sanctified Bloodmage Gloves
      {ICCSetWeapons, 51179}, // Sanctified Crimson Acolyte Gloves
      {ICCSetWeapons, 51183}, // Sanctified Crimson Acolyte Handwraps
      {ICCSetWeapons, 51209}, // Sanctified Dark Coven Gloves
      {ICCSetWeapons, 51201}, // Sanctified Frost Witch's Gloves
      {ICCSetWeapons, 51196}, // Sanctified Frost Witch's Grips
      {ICCSetWeapons, 51191}, // Sanctified Frost Witch's Handguards
      {ICCSetWeapons, 51138}, // Sanctified Lasherweave Gauntlets
      {ICCSetWeapons, 51148}, // Sanctified Lasherweave Gloves
      {ICCSetWeapons, 51144}, // Sanctified Lasherweave Handgrips
      {ICCSetWeapons, 51163}, // Sanctified Lightsworn Gauntlets
      {ICCSetWeapons, 51169}, // Sanctified Lightsworn Gloves
      {ICCSetWeapons, 51172}, // Sanctified Lightsworn Handguards
      {ICCSetWeapons, 51128}, // Sanctified Scourgelord Gauntlets
      {ICCSetWeapons, 51132}, // Sanctified Scourgelord Handguards
      {ICCSetWeapons, 51188}, // Sanctified Shadowblade Gauntlets
      {ICCSetWeapons, 51213}, // Sanctified Ymirjar Lord's Gauntlets
      {ICCSetWeapons, 51217}, // Sanctified Ymirjar Lord's Handguards

      // == ICC / Lich King weapons (ilvl 277) ==
      // One-Hand
      {ICCSetWeapons, 50672}, // Bloodvenom Blade
      {ICCSetWeapons, 50641}, // Heartpierce
      {ICCSetWeapons, 50708}, // Last Word
      {ICCSetWeapons, 50621}, // Lungbreaker
      {ICCSetWeapons, 50676}, // Rib Spreader
      {ICCSetWeapons, 50654}, // Scourgeborne Waraxe
      // Shield
      {ICCSetWeapons, 50616}, // Bulwark of Smouldering Steel
      {ICCSetWeapons, 50729}, // Icecrown Glacial Wall
      // Ranged
      {ICCSetWeapons, 50684}, // Corpse-Impaling Spike
      {ICCSetWeapons, 50631}, // Nightmare Ender
      {ICCSetWeapons, 50638}, // Zod's Repeating Longbow
      // Two-Hand
      {ICCSetWeapons, 50727}, // Bloodfall
      {ICCSetWeapons, 50709}, // Bryntroll, the Bone Arbiter
      {ICCSetWeapons, 50603}, // Cryptmaker
      {ICCSetWeapons, 50695}, // Distant Land
      {ICCSetWeapons, 50725}, // Dying Light
      {ICCSetWeapons, 50648}, // Nibelung
      // Main Hand
      {ICCSetWeapons, 50692}, // Black Bruise
      {ICCSetWeapons, 50608}, // Frozen Bonespike
      {ICCSetWeapons, 50704}, // Rigormortis
      {ICCSetWeapons, 50685}, // Trauma
      // Off Hand
      {ICCSetWeapons, 50710}, // Keleseth's Seducer
      // Held In Off-Hand
      {ICCSetWeapons, 50719}, // Shadow Silk Spindle
      {ICCSetWeapons, 50635}, // Sundial of Eternal Dusk
      // Thrown
      {ICCSetWeapons, 51880}, // Gluth's Fetching Knife

      // == ICC relics (ilvl 264) ==
      {ICCSetWeapons, 50454}, // Idol of the Black Willow
      {ICCSetWeapons, 50455}, // Libram of Three Truths
      {ICCSetWeapons, 50456}, // Idol of the Crying Moon
      {ICCSetWeapons, 50457}, // Idol of the Lunar Eclipse
      {ICCSetWeapons, 50458}, // Bizuri's Totem of Shattered Ice
      {ICCSetWeapons, 50459}, // Sigil of the Hanged Man
      {ICCSetWeapons, 50460}, // Libram of Blinding Light
      {ICCSetWeapons, 50461}, // Libram of the Eternal Tower
      {ICCSetWeapons, 50462}, // Sigil of the Bone Gryphon
      {ICCSetWeapons, 50463}, // Totem of the Avalanche
      {ICCSetWeapons, 50464}, // Totem of the Surging Sea

      // == ICC offparts (ilvl 264; + ToGC 272 cloaks) ==
      // Neck
      {ICCOffparts, 49989}, // Ahn'kahar Onyx Neckguard
      {ICCOffparts, 50005}, // Amulet of the Silent Eulogy
      {ICCOffparts, 50023}, // Bile-Encrusted Medallion
      {ICCOffparts, 50182}, // Blood Queen's Crimson Choker
      {ICCOffparts, 49975}, // Bone Sentinel's Amulet
      {ICCOffparts, 51871}, // Choker of Filthy Diamonds
      {ICCOffparts, 51842}, // Collar of Haughty Disdain
      {ICCOffparts, 50061}, // Holiday's Grace
      {ICCOffparts, 51867}, // Infected Choker
      {ICCOffparts, 50180}, // Lana'thel's Chain of Flagellation
      {ICCOffparts, 51934}, // Marrowgar's Scratching Choker
      {ICCOffparts, 50195}, // Noose of Malachite
      {ICCOffparts, 51863}, // Pendant of Split Veins
      {ICCOffparts, 51890}, // Precious's Putrid Collar
      {ICCOffparts, 51822}, // Rimetooth Pendant
      {ICCOffparts, 50421}, // Sindragosa's Cruel Claw
      {ICCOffparts, 51894}, // Soulcleave Pendant
      {ICCOffparts, 50452}, // Wodin's Lucky Necklace
      // Waist
      {ICCOffparts, 50067}, // Astrylian's Sutured Cinch
      {ICCOffparts, 50993}, // Band of the Night Raven
      {ICCOffparts, 50036}, // Belt of Broken Bones
      {ICCOffparts, 50996}, // Belt of Omission
      {ICCOffparts, 50994}, // Belt of Petrified Ivy
      {ICCOffparts, 50015}, // Belt of the Blood Nova
      {ICCOffparts, 50451}, // Belt of the Lonely Noble
      {ICCOffparts, 51853}, // Blood-Drinker's Girdle
      {ICCOffparts, 51862}, // Cauterized Cord
      {ICCOffparts, 50997}, // Circle of Ossus
      {ICCOffparts, 50187}, // Coldwraith Links
      {ICCOffparts, 51908}, // Cord of Dark Suffering
      {ICCOffparts, 51930}, // Cord of the Patronizing Practitioner
      {ICCOffparts, 49978}, // Crushing Coldwraith Belt
      {ICCOffparts, 51919}, // Deathspeaker Disciple's Belt
      {ICCOffparts, 51821}, // Etched Dragonbone Girdle
      {ICCOffparts, 51879}, // Flesh-Shaper's Gurney Strap
      {ICCOffparts, 51831}, // Ironrope Belt of Ymirjar
      {ICCOffparts, 50989}, // Lich Killer's Lanyard
      {ICCOffparts, 50063}, // Lingering Illness
      {ICCOffparts, 51935}, // Linked Scourge Vertebrae
      {ICCOffparts, 50987}, // Malevolent Girdle
      {ICCOffparts, 50413}, // Nerub'ar Stalker's Cord
      {ICCOffparts, 50069}, // Professor's Bloodied Smock
      {ICCOffparts, 51925}, // Soulthief's Braided Belt
      {ICCOffparts, 51836}, // Tightening Waistband
      {ICCOffparts, 50995}, // Vengeful Noose
      {ICCOffparts, 50991}, // Verdigris Chain Belt
      {ICCOffparts, 50992}, // Waistband of Despair
      {ICCOffparts, 50010}, // Waistband of Righteous Fury
      // Feet
      {ICCOffparts, 51931}, // Ancient Skeletal Boots
      {ICCOffparts, 49894}, // Blessed Cenarion Boots
      {ICCOffparts, 49983}, // Blood-Soaked Saronite Stompers
      {ICCOffparts, 51915}, // Bone Drake's Enameled Boots
      {ICCOffparts, 49907}, // Boots of Kingly Upheaval
      {ICCOffparts, 51920}, // Boots of the Frozen Seed
      {ICCOffparts, 50416}, // Boots of the Funeral March
      {ICCOffparts, 50009}, // Boots of Unnatural Growth
      {ICCOffparts, 49890}, // Deathfrost Boots
      {ICCOffparts, 49896}, // Earthsoul Boots
      {ICCOffparts, 49895}, // Footpads of Impending Death
      {ICCOffparts, 49950}, // Frostbitten Fur Boots
      {ICCOffparts, 50190}, // Grinning Skull Greatboots
      {ICCOffparts, 49906}, // Hellfrozen Bonegrinders
      {ICCOffparts, 51899}, // Icecrown Spire Sandals
      {ICCOffparts, 49993}, // Necrophotic Greaves
      {ICCOffparts, 51850}, // Pale Corpse Boots
      {ICCOffparts, 50062}, // Plague Scientist's Boots
      {ICCOffparts, 49905}, // Protectors of Life
      {ICCOffparts, 49897}, // Rock-Steady Treads
      {ICCOffparts, 49893}, // Sandals of Consecration
      {ICCOffparts, 51816}, // Scourge Fanged Stompers
      {ICCOffparts, 51873}, // Shuffling Shoes
      {ICCOffparts, 51856}, // Taldaram's Soft Slippers
      {ICCOffparts, 51891}, // Taldron's Long Neglected Boots
      {ICCOffparts, 50071}, // Treads of the Wasteland
      {ICCOffparts, 51818}, // Wyrmwing Treads
      // Wrists
      {ICCOffparts, 50030}, // Bloodsunder's Bracers
      {ICCOffparts, 51918}, // Bracers of Dark Blessings
      {ICCOffparts, 49960}, // Bracers of Dark Reckoning
      {ICCOffparts, 50417}, // Bracers of Eternal Dreaming
      {ICCOffparts, 51907}, // Bracers of Pale Illumination
      {ICCOffparts, 51929}, // Coldwraith Bracers
      {ICCOffparts, 50175}, // Crypt Keeper's Bracers
      {ICCOffparts, 50032}, // Death Surgeon's Sleeves
      {ICCOffparts, 51872}, // Ether-Soaked Bracers
      {ICCOffparts, 51901}, // Gargoyle Spit Bracers
      {ICCOffparts, 51914}, // Icecrown Rampart Bracers
      {ICCOffparts, 50002}, // Polar Bear Claw Bracers
      {ICCOffparts, 50000}, // Scourge Hunter's Vambraces
      {ICCOffparts, 51832}, // Taiga Bindings
      {ICCOffparts, 49994}, // The Lady's Brittle Bracers
      {ICCOffparts, 50333}, // Toskk's Maximized Wristguards
      {ICCOffparts, 51820}, // Vambraces of the Frost Wyrm Queen
      {ICCOffparts, 51885}, // Wrists of Septic Shock
      // Finger
      {ICCOffparts, 51913}, // Abomination's Bloody Ring
      {ICCOffparts, 49949}, // Band of the Bone Colossus
      {ICCOffparts, 51849}, // Cerise Coiled Ring
      {ICCOffparts, 50185}, // Devium's Eternally Cold Ring
      {ICCOffparts, 50186}, // Frostbrood Sapphire Ring
      {ICCOffparts, 50447}, // Harbinger's Bone Band
      {ICCOffparts, 50174}, // Incarnadine Band of Mending
      {ICCOffparts, 49985}, // Juggernaut Band
      {ICCOffparts, 49977}, // Loop of the Endless Labyrinth
      {ICCOffparts, 49967}, // Marrowgar's Frigid Eye
      {ICCOffparts, 50424}, // Memory of Malygos
      {ICCOffparts, 50414}, // Might of Blight
      {ICCOffparts, 49990}, // Ring of Maddening Whispers
      {ICCOffparts, 50008}, // Ring of Rapid Ascent
      {ICCOffparts, 50453}, // Ring of Rotting Sinew
      {ICCOffparts, 51878}, // Rotface's Rupturing Ring
      {ICCOffparts, 51900}, // Saurfang's Cold-Forged Band
      {ICCOffparts, 50025}, // Seal of Many Mouths
      {ICCOffparts, 51843}, // Seal of the Twilight Queen
      {ICCOffparts, 51884}, // Signet of Putrefaction
      {ICCOffparts, 49999}, // Skeleton Lord's Circle
      {ICCOffparts, 51855}, // Thrice Fanged Signet
      {ICCOffparts, 50170}, // Valanar's Other Signet Ring
      // Back
      {ICCOffparts, 51888}, // Cloak of Many Skins
      {ICCOffparts, 50468}, // Drape of the Violet Tower
      {ICCOffparts, 50205}, // Frostbinder's Shredded Cape
      {ICCOffparts, 50014}, // Greatcloak of the Turned Champion
      {ICCOffparts, 51848}, // Heartsick Mender's Cape
      {ICCOffparts, 51826}, // Lich Wrappings
      {ICCOffparts, 50467}, // Might of the Ocean Serpent
      {ICCOffparts, 50470}, // Recovered Scarlet Onslaught Cape
      {ICCOffparts, 50074}, // Royal Crimson Cloak
      {ICCOffparts, 51912}, // Saronite Gargoyle Cloak
      {ICCOffparts, 50466}, // Sentinel's Winter Cloak
      {ICCOffparts, 49998}, // Shadowvault Slayer's Cloak
      {ICCOffparts, 51933}, // Shawl of Nerubian Silk
      {ICCOffparts, 50469}, // Volde's Cloak of the Night Sky
      {ICCOffparts, 50019}, // Winding Sheet
      // Back (ToGC 272)
      {ICCOffparts, 47545}, // Vereesa's Dexterity
      {ICCOffparts, 47546}, // Sylvanas' Cunning
      {ICCOffparts, 47547}, // Varian's Furor
      {ICCOffparts, 47548}, // Garrosh's Rage
      {ICCOffparts, 47549}, // Magni's Resolution
      {ICCOffparts, 47550}, // Cairne's Endurance
      {ICCOffparts, 47551}, // Aethas' Intensity
      {ICCOffparts, 47552}, // Jaina's Radiance
      {ICCOffparts, 47553}, // Bolvar's Devotion
      {ICCOffparts, 47554}, // Lady Liadrin's Conviction

      // == ICC offset: non-set main pieces (ilvl 264) ==
      // Head
      {ICCOffset, 49986}, // Broken Ram Skull Helm
      {ICCOffset, 50006}, // Corp'rethar Ceremonial Crown
      {ICCOffset, 51837}, // Cowl of Malefic Repose
      {ICCOffset, 51924}, // Deathspeaker Zealot's Helm
      {ICCOffset, 51866}, // Discarded Bag of Entrails
      {ICCOffset, 50060}, // Faceplate of the Forgotten
      {ICCOffset, 50073}, // Geistlord's Punishment Sack
      {ICCOffset, 50026}, // Helm of the Elder Moon
      {ICCOffset, 51906}, // Ice-Reinforced Vrykul Helm
      {ICCOffset, 50072}, // Landsoul's Horned Greathelm
      {ICCOffset, 51825}, // Sister Svalna's Spangenhelm
      {ICCOffset, 49952}, // Snowserpent Mail Helm
      {ICCOffset, 50202}, // Snowstorm Helm
      {ICCOffset, 51877}, // Taldron's Short-Sighted Helm
      {ICCOffset, 51896}, // Thaumaturge's Crackling Cowl
      // Shoulders
      {ICCOffset, 51883}, // Bloodstained Surgeon's Shoulderguards
      {ICCOffset, 50003}, // Boneguard Commander's Pauldrons
      {ICCOffset, 49987}, // Cultist's Bloodsoaked Spaulders
      {ICCOffset, 50022}, // Dual-Bladed Pauldrons
      {ICCOffset, 51824}, // Emerald Saint's Spaulders
      {ICCOffset, 50059}, // Horrific Flesh Epaulets
      {ICCOffset, 51911}, // Pauldrons of Lost Hope
      {ICCOffset, 50020}, // Raging Behemoth's Shoulderplates
      {ICCOffset, 49980}, // Rusted Bonespike Pauldrons
      {ICCOffset, 51865}, // Scalpel-Sharpening Shoulderguards
      {ICCOffset, 51811}, // Shoulderguards of Crystalline Bone
      {ICCOffset, 51864}, // Shoulderpads of the Morbid Ritual
      {ICCOffset, 51839}, // Shoulderpads of the Searing Kiss
      {ICCOffset, 50171}, // Shoulders of Frost-Tipped Thorns
      {ICCOffset, 49991}, // Shoulders of Mercy Killing
      {ICCOffset, 51859}, // Shoulders of Ruinous Senility
      {ICCOffset, 51830}, // Skinned Whelp Shoulders
      {ICCOffset, 51847}, // Spaulders of the Blood Princes
      {ICCOffset, 50449}, // Stiffened Corpse Shoulderpads
      // Chest
      {ICCOffset, 51902}, // Blade-Scored Carapace
      {ICCOffset, 50024}, // Blightborne Warplate
      {ICCOffset, 51851}, // Bloodsoul Raiment
      {ICCOffset, 50038}, // Carapace of Forgotten Kings
      {ICCOffset, 50965}, // Castle Breaker's Battleplate
      {ICCOffset, 50968}, // Cataclysmic Chestguard
      {ICCOffset, 51840}, // Chestguard of Siphoned Elements
      {ICCOffset, 51870}, // Chestguard of the Failed Experiment
      {ICCOffset, 51923}, // Chestguard of the Frigid Noose
      {ICCOffset, 51861}, // Chestplate of Septic Stitches
      {ICCOffset, 50969}, // Chestplate of Unspoken Truths
      {ICCOffset, 49996}, // Deathwhisper Raiment
      {ICCOffset, 50975}, // Ermine Coronation Robes
      {ICCOffset, 49951}, // Gendarme's Cuirass
      {ICCOffset, 51917}, // Ghoul Commander's Cuirass
      {ICCOffset, 51903}, // Hauberk of a Thousand Cuts
      {ICCOffset, 50001}, // Ikfirus's Sack of Wonder
      {ICCOffset, 50970}, // Longstrider's Vest
      {ICCOffset, 50177}, // Mail of Crimson Coins
      {ICCOffset, 50971}, // Mail of the Geyser
      {ICCOffset, 50974}, // Meteor Chaser's Raiment
      {ICCOffset, 50418}, // Robe of the Waking Nightmare
      {ICCOffset, 51813}, // Robes of Azure Downfall
      {ICCOffset, 50027}, // Rot-Resistant Breastplate
      {ICCOffset, 50172}, // Sanguine Silk Robes
      {ICCOffset, 50972}, // Shadow Seeker's Tunic
      {ICCOffset, 50973}, // Vestments of Spruce and Fir
      // Legs
      {ICCOffset, 51854}, // Battle-Maiden's Legguards
      {ICCOffset, 49899}, // Bladeborn Leggings
      {ICCOffset, 51928}, // Corrupted Silverplate Leggings
      {ICCOffset, 51895}, // Deathforged Legplates
      {ICCOffset, 49901}, // Draconic Bonesplinter Legguards
      {ICCOffset, 50042}, // Gangrenous Leggings
      {ICCOffset, 51841}, // Ivory-Inlaid Leggings
      {ICCOffset, 51882}, // Kilt of Untreated Wounds
      {ICCOffset, 50041}, // Leather of Stitched Scourge Parts
      {ICCOffset, 50450}, // Leggings of Dubious Charms
      {ICCOffset, 50199}, // Leggings of Dying Candles
      {ICCOffset, 49988}, // Leggings of Northern Lights
      {ICCOffset, 51823}, // Leggings of the Refracted Mind
      {ICCOffset, 51897}, // Leggings of Unrelenting Blood
      {ICCOffset, 49891}, // Leggings of Woven Death
      {ICCOffset, 49964}, // Legguards of Lost Hope
      {ICCOffset, 51829}, // Legguards of the Twisted Dream
      {ICCOffset, 51817}, // Legplates of Aetheric Strife
      {ICCOffset, 49903}, // Legplates of Painful Death
      {ICCOffset, 49898}, // Legwraps of Unleashed Nature
      {ICCOffset, 49900}, // Lightning-Infused Leggings
      {ICCOffset, 49892}, // Lightweave Leggings
      {ICCOffset, 49904}, // Pillars of Might
      {ICCOffset, 51889}, // Plague-Soaked Leather Leggings
      {ICCOffset, 50056}, // Plaguebringer's Stained Pants
      {ICCOffset, 49902}, // Puresteel Legplates
      {ICCOffset, 51860}, // Rippling Flesh Kilt
      {ICCOffset, 50192}, // Scourge Reaver's Legplates
      // Hands
      {ICCOffset, 50021}, // Aldriana's Gloves of Secrecy
      {ICCOffset, 50188}, // Anub'ar Stalker's Gloves
      {ICCOffset, 50980}, // Blizzard Keeper's Mitts
      {ICCOffset, 50982}, // Cat Burglar's Grips
      {ICCOffset, 49995}, // Fallen Lord's Handguards
      {ICCOffset, 51886}, // Festergut's Gaseous Gloves
      {ICCOffset, 51892}, // Festering Fingerguards
      {ICCOffset, 50037}, // Fleshrending Gauntlets
      {ICCOffset, 50977}, // Gatecrasher's Gauntlets
      {ICCOffset, 50976}, // Gauntlets of Overexposure
      {ICCOffset, 50978}, // Gauntlets of the Kraken
      {ICCOffset, 50984}, // Gloves of Ambivalence
      {ICCOffset, 51874}, // Gloves of Broken Fingers
      {ICCOffset, 50983}, // Gloves of False Gestures
      {ICCOffset, 50981}, // Gloves of the Great Horned Owl
      {ICCOffset, 50011}, // Gunship Captain's Mittens
      {ICCOffset, 51926}, // Handgrips of Frost and Sleet
      {ICCOffset, 49979}, // Handguards of Winter's Respite
      {ICCOffset, 51814}, // Icicle Shapers
      {ICCOffset, 50979}, // Logsplitters
      {ICCOffset, 50176}, // San'layn Ritualist Gloves
      {ICCOffset, 51904}, // Scourge Stranglers
      {ICCOffset, 51921}, // Sister's Handshrouds
      {ICCOffset, 51827}, // Stormbringer Gloves
      {ICCOffset, 50075}, // Taldaram's Plated Fists
      {ICCOffset, 51844}, // Throatrender Handguards
      {ICCOffset, 50064}, // Unclean Surgical Gloves
      {ICCOffset, 51835}, // Veincrusher Gauntlets
  };
  return items;
}
} // namespace arenacraft
