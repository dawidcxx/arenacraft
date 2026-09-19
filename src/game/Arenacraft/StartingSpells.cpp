#include "CharacterCreation.hpp"

#include "Player.h"
#include "SharedDefines.h"

#include <unordered_map>
#include <vector>

namespace arenacraft
{
namespace
{
// Full level-80 ability set (every rank) pre-learned at character creation so
// an instant-80 character never has to visit a class trainer. Built from the
// class trainer spell lists plus the auto/quest abilities trainers do not
// carry (hunter pet commands, druid forms, warlock summons, mage teleports,
// warrior stances, ...). Ranks of one ability are kept adjacent, in order.
std::unordered_map<uint8, std::vector<uint32>> const ClassSpells = {
    {CLASS_WARRIOR,
     {
         71, // Defensive Stance
         72, // Shield Bash
         // Heroic Strike
         78,    // rank 1
         284,   // rank 2
         285,   // rank 3
         1608,  // rank 4
         11564, // rank 5
         11565, // rank 6
         11566, // rank 7
         11567, // rank 8
         25286, // rank 9
         29707, // rank 10
         30324, // rank 11
         47449, // rank 12
         47450, // rank 13
         // Charge
         100,   // rank 1
         6178,  // rank 2
         11578, // rank 3
         355,   // Taunt
         // Commanding Shout
         469,   // rank 1
         47439, // rank 2
         47440, // rank 3
         674,   // Dual Wield
         676,   // Disarm
         694,   // Mocking Blow
         750,   // Plate Mail
         // Rend
         772,   // rank 1
         6546,  // rank 2
         6547,  // rank 3
         6548,  // rank 4
         11572, // rank 5
         11573, // rank 6
         11574, // rank 7
         25208, // rank 8
         46845, // rank 9
         47465, // rank 10
         // Cleave
         845,   // rank 1
         7369,  // rank 2
         11608, // rank 3
         11609, // rank 4
         20569, // rank 5
         25231, // rank 6
         47519, // rank 7
         47520, // rank 8
         871,   // Shield Wall
         // Demoralizing Shout
         1160,  // rank 1
         6190,  // rank 2
         11554, // rank 3
         11555, // rank 4
         11556, // rank 5
         25202, // rank 6
         25203, // rank 7
         47437, // rank 8
         1161,  // Challenging Shout
         // Slam
         1464,  // rank 1
         8820,  // rank 2
         11604, // rank 3
         11605, // rank 4
         25241, // rank 5
         25242, // rank 6
         47474, // rank 7
         47475, // rank 8
         1680,  // Whirlwind
         1715,  // Hamstring
         1719,  // Recklessness
         2457,  // Battle Stance
         2458,  // Berserker Stance
         2565,  // Shield Block
         2687,  // Bloodrage
         3127,  // Parry
         3411,  // Intervene
         5246,  // Intimidating Shout
         // Execute
         5308,  // rank 1
         20658, // rank 2
         20660, // rank 3
         20661, // rank 4
         20662, // rank 5
         25234, // rank 6
         25236, // rank 7
         47470, // rank 8
         47471, // rank 9
         // Thunder Clap
         6343,  // rank 1
         8198,  // rank 2
         8204,  // rank 3
         8205,  // rank 4
         11580, // rank 5
         11581, // rank 6
         25264, // rank 7
         47501, // rank 8
         47502, // rank 9
         6552,  // Pummel
         // Revenge
         6572,  // rank 1
         6574,  // rank 2
         7379,  // rank 3
         11600, // rank 4
         11601, // rank 5
         25288, // rank 6
         25269, // rank 7
         30357, // rank 8
         57823, // rank 9
         // Battle Shout
         6673,  // rank 1
         5242,  // rank 2
         6192,  // rank 3
         11549, // rank 4
         11550, // rank 5
         11551, // rank 6
         25289, // rank 7
         2048,  // rank 8
         47436, // rank 9
         7376,  // Defensive Stance Passive
         7381,  // Berserker Stance Passive
         7384,  // Overpower
         7386,  // Sunder Armor
         12678, // Stance Mastery
         18499, // Berserker Rage
         20230, // Retaliation
         20252, // Intercept
         21156, // Battle Stance Passive
         23920, // Spell Reflection
         // Shield Slam
         23922, // rank 1
         23923, // rank 2
         23924, // rank 3
         23925, // rank 4
         25258, // rank 5
         30356, // rank 6
         47487, // rank 7
         47488, // rank 8
         34428, // Victory Rush
         55694, // Enraged Regeneration
         57755, // Heroic Throw
         64382, // Shattering Throw
     }},
    {CLASS_PALADIN,
     {
         // Devotion Aura
         465,   // rank 1
         10290, // rank 2
         643,   // rank 3
         10291, // rank 4
         1032,  // rank 5
         10292, // rank 6
         10293, // rank 7
         27149, // rank 8
         48941, // rank 9
         48942, // rank 10
         498,   // Divine Protection
              // Lay on Hands
         633,   // rank 1
         2800,  // rank 2
         10310, // rank 3
         27154, // rank 4
         48788, // rank 5
                // Holy Light
         635,   // rank 1
         639,   // rank 2
         647,   // rank 3
         1026,  // rank 4
         1042,  // rank 5
         3472,  // rank 6
         10328, // rank 7
         10329, // rank 8
         25292, // rank 9
         27135, // rank 10
         27136, // rank 11
         48781, // rank 12
         48782, // rank 13
         642,   // Divine Shield
         750,   // Plate Mail
              // Hammer of Justice
         853,   // rank 1
         5588,  // rank 2
         5589,  // rank 3
         10308, // rank 4
                // Exorcism
         879,   // rank 1
         5614,  // rank 2
         5615,  // rank 3
         10312, // rank 4
         10313, // rank 5
         10314, // rank 6
         27138, // rank 7
         48800, // rank 8
         48801, // rank 9
                // Hand of Protection
         1022,  // rank 1
         5599,  // rank 2
         10278, // rank 3
         1038,  // Hand of Salvation
         1044,  // Hand of Freedom
         1152,  // Purify
               // Holy Wrath
         2812,  // rank 1
         10318, // rank 2
         27139, // rank 3
         48816, // rank 4
         48817, // rank 5
         3127,  // Parry
         4987,  // Cleanse
         5502,  // Sense Undead
         6940,  // Hand of Sacrifice
               // Retribution Aura
         7294,  // rank 1
         10298, // rank 2
         10299, // rank 3
         10300, // rank 4
         10301, // rank 5
         27150, // rank 6
         54043, // rank 7
                // Redemption
         7328,  // rank 1
         10322, // rank 2
         10324, // rank 3
         20772, // rank 4
         20773, // rank 5
         48949, // rank 6
         48950, // rank 7
         10326, // Turn Evil
         13819, // Warhorse
         13820, // Summon Warhorse
                // Blessing of Might
         19740, // rank 1
         19834, // rank 2
         19835, // rank 3
         19836, // rank 4
         19837, // rank 5
         19838, // rank 6
         25291, // rank 7
         27140, // rank 8
         48931, // rank 9
         48932, // rank 10
                // Blessing of Wisdom
         19742, // rank 1
         19850, // rank 2
         19852, // rank 3
         19853, // rank 4
         19854, // rank 5
         25290, // rank 6
         27142, // rank 7
         48935, // rank 8
         48936, // rank 9
         19746, // Concentration Aura
                // Flash of Light
         19750, // rank 1
         19939, // rank 2
         19940, // rank 3
         19941, // rank 4
         19942, // rank 5
         19943, // rank 6
         27137, // rank 7
         48784, // rank 8
         48785, // rank 9
         19752, // Divine Intervention
                // Shadow Resistance Aura
         19876, // rank 1
         19895, // rank 2
         19896, // rank 3
         27151, // rank 4
         48943, // rank 5
                // Frost Resistance Aura
         19888, // rank 1
         19897, // rank 2
         19898, // rank 3
         27152, // rank 4
         48945, // rank 5
                // Fire Resistance Aura
         19891, // rank 1
         19899, // rank 2
         19900, // rank 3
         27153, // rank 4
         48947, // rank 5
         20164, // Seal of Justice
         20165, // Seal of Light
         20166, // Seal of Wisdom
         20217, // Blessing of Kings
         20271, // Judgement of Light
         23214, // Charger
                // Hammer of Wrath
         24275, // rank 1
         24274, // rank 2
         24239, // rank 3
         27180, // rank 4
         48805, // rank 5
         48806, // rank 6
         25780, // Righteous Fury
                // Greater Blessing of Might
         25782, // rank 1
         25916, // rank 2
         27141, // rank 3
         48933, // rank 4
         48934, // rank 5
                // Greater Blessing of Wisdom
         25894, // rank 1
         25918, // rank 2
         27143, // rank 3
         48937, // rank 4
         48938, // rank 5
         25898, // Greater Blessing of Kings
         25899, // Greater Blessing of Sanctuary
                // Consecration
         26573, // rank 1
         20116, // rank 2
         20922, // rank 3
         20923, // rank 4
         20924, // rank 5
         27173, // rank 6
         48818, // rank 7
         48819, // rank 8
         31789, // Righteous Defense
         31801, // Seal of Vengeance
         31884, // Avenging Wrath
         32223, // Crusader Aura
         34767, // Summon Charger
         34769, // Summon Warhorse
         53407, // Judgement of Justice
         53408, // Judgement of Wisdom
                // Shield of Righteousness
         53600, // rank 1
         61411, // rank 2
         53601, // Sacred Shield
         53651, // Light's Beacon
         53736, // Seal of Corruption
         54428, // Divine Plea
         62124, // Hand of Reckoning
     }},
    {CLASS_HUNTER,
     {
         // Mend Pet
         136,   // rank 1
         3111,  // rank 2
         3661,  // rank 3
         3662,  // rank 4
         13542, // rank 5
         13543, // rank 6
         13544, // rank 7
         27046, // rank 8
         48989, // rank 9
         48990, // rank 10
         674,   // Dual Wield
         781,   // Disengage
         883,   // Call Pet
         982,   // Revive Pet
         1002,  // Eyes of the Beast
               // Hunter's Mark
         1130,  // rank 1
         14323, // rank 2
         14324, // rank 3
         14325, // rank 4
         53338, // rank 5
         1462,  // Beast Lore
         1494,  // Track Beasts
               // Mongoose Bite
         1495,  // rank 1
         14269, // rank 2
         14270, // rank 3
         14271, // rank 4
         36916, // rank 5
         53339, // rank 6
                // Freezing Trap
         1499,  // rank 1
         14310, // rank 2
         14311, // rank 3
                // Volley
         1510,  // rank 1
         14294, // rank 2
         14295, // rank 3
         27022, // rank 4
         58431, // rank 5
         58434, // rank 6
                // Scare Beast
         1513,  // rank 1
         14326, // rank 2
         14327, // rank 3
         1515,  // Tame Beast
         1543,  // Flare
               // Serpent Sting
         1978,  // rank 1
         13549, // rank 2
         13550, // rank 3
         13551, // rank 4
         13552, // rank 5
         13553, // rank 6
         13554, // rank 7
         13555, // rank 8
         25295, // rank 9
         27016, // rank 10
         49000, // rank 11
         49001, // rank 12
         2641,  // Dismiss Pet
               // Multi-Shot
         2643,  // rank 1
         14288, // rank 2
         14289, // rank 3
         14290, // rank 4
         25294, // rank 5
         27021, // rank 6
         49047, // rank 7
         49048, // rank 8
                // Raptor Strike
         2973,  // rank 1
         14260, // rank 2
         14261, // rank 3
         14262, // rank 4
         14263, // rank 5
         14264, // rank 6
         14265, // rank 7
         14266, // rank 8
         27014, // rank 9
         48995, // rank 10
         48996, // rank 11
         2974,  // Wing Clip
         3034,  // Viper Sting
         3043,  // Scorpid Sting
               // Arcane Shot
         3044,  // rank 1
         14281, // rank 2
         14282, // rank 3
         14283, // rank 4
         14284, // rank 5
         14285, // rank 6
         14286, // rank 7
         14287, // rank 8
         27019, // rank 9
         49044, // rank 10
         49045, // rank 11
         3045,  // Rapid Fire
         3127,  // Parry
         5116,  // Concussive Shot
         5118,  // Aspect of the Cheetah
         5384,  // Feign Death
         6197,  // Eagle Eye
         6991,  // Feed Pet
         8737,  // Mail
         13159, // Aspect of the Pack
         13161, // Aspect of the Beast
         13163, // Aspect of the Monkey
                // Aspect of the Hawk
         13165, // rank 1
         14318, // rank 2
         14319, // rank 3
         14320, // rank 4
         14321, // rank 5
         14322, // rank 6
         25296, // rank 7
         27044, // rank 8
         13481, // Tame Beast
                // Immolation Trap
         13795, // rank 1
         14302, // rank 2
         14303, // rank 3
         14304, // rank 4
         14305, // rank 5
         27023, // rank 6
         49055, // rank 7
         49056, // rank 8
         13809, // Frost Trap
                // Explosive Trap
         13813, // rank 1
         14316, // rank 2
         14317, // rank 3
         27025, // rank 4
         49066, // rank 5
         49067, // rank 6
         19263, // Deterrence
         19801, // Tranquilizing Shot
         19878, // Track Demons
         19879, // Track Dragonkin
         19880, // Track Elementals
         19882, // Track Giants
         19883, // Track Humanoids
         19884, // Track Undead
         19885, // Track Hidden
                // Aspect of the Wild
         20043, // rank 1
         20190, // rank 2
         27045, // rank 3
         49071, // rank 4
         20736, // Distracting Shot
                // Wyvern Sting
         24131, // rank 1
         24134, // rank 2
         24135, // rank 3
         27069, // rank 4
         49009, // rank 5
         49010, // rank 6
         34026, // Kill Command
         34074, // Aspect of the Viper
         34477, // Misdirection
         34600, // Snake Trap
         53271, // Master's Call
                // Kill Shot
         53351, // rank 1
         61005, // rank 2
         61006, // rank 3
                // Steady Shot
         56641, // rank 1
         34120, // rank 2
         49051, // rank 3
         49052, // rank 4
         60192, // Freezing Arrow
                // Aspect of the Dragonhawk
         61846, // rank 1
         61847, // rank 2
         62757, // Call Stabled Pet
     }},
    {CLASS_ROGUE,
     {
         // Backstab
         53,    // rank 1
         2589,  // rank 2
         2590,  // rank 3
         2591,  // rank 4
         8721,  // rank 5
         11279, // rank 6
         11280, // rank 7
         11281, // rank 8
         25300, // rank 9
         26863, // rank 10
         48656, // rank 11
         48657, // rank 12
                // Kidney Shot
         408,  // rank 1
         8643, // rank 2
         674,  // Dual Wield
              // Garrote
         703,   // rank 1
         8631,  // rank 2
         8632,  // rank 3
         8633,  // rank 4
         11289, // rank 5
         11290, // rank 6
         26839, // rank 7
         26884, // rank 8
         48675, // rank 9
         48676, // rank 10
         921,   // Pick Pocket
         1725,  // Distract
               // Sinister Strike
         1752,  // rank 1
         1757,  // rank 2
         1758,  // rank 3
         1759,  // rank 4
         1760,  // rank 5
         8621,  // rank 6
         11293, // rank 7
         11294, // rank 8
         26861, // rank 9
         26862, // rank 10
         48637, // rank 11
         48638, // rank 12
         1766,  // Kick
         1776,  // Gouge
         1784,  // Stealth
         1804,  // Pick Lock
         1833,  // Cheap Shot
         1842,  // Disarm Trap
               // Vanish
         1856,  // rank 1
         1857,  // rank 2
         26889, // rank 3
         1860,  // Safe Fall
               // Rupture
         1943,  // rank 1
         8639,  // rank 2
         8640,  // rank 3
         11273, // rank 4
         11274, // rank 5
         11275, // rank 6
         26867, // rank 7
         48671, // rank 8
         48672, // rank 9
                // Feint
         1966,  // rank 1
         6768,  // rank 2
         8637,  // rank 3
         11303, // rank 4
         25302, // rank 5
         27448, // rank 6
         48658, // rank 7
         48659, // rank 8
         2094,  // Blind
               // Eviscerate
         2098,  // rank 1
         6760,  // rank 2
         6761,  // rank 3
         6762,  // rank 4
         8623,  // rank 5
         8624,  // rank 6
         11299, // rank 7
         11300, // rank 8
         31016, // rank 9
         26865, // rank 10
         48667, // rank 11
         48668, // rank 12
         2836,  // Detect Traps
               // Sprint
         2983,  // rank 1
         8696,  // rank 2
         11305, // rank 3
         3127,  // Parry
               // Slice and Dice
         5171, // rank 1
         6774, // rank 2
               // Evasion
         5277,  // rank 1
         26669, // rank 2
         5938,  // Shiv
               // Sap
         6770,  // rank 1
         2070,  // rank 2
         11297, // rank 3
         51724, // rank 4
         8647,  // Expose Armor
               // Ambush
         8676,  // rank 1
         8724,  // rank 2
         8725,  // rank 3
         11267, // rank 4
         11268, // rank 5
         11269, // rank 6
         27441, // rank 7
         48689, // rank 8
         48690, // rank 9
         48691, // rank 10
                // Deadly Throw
         26679, // rank 1
         48673, // rank 2
         48674, // rank 3
         31224, // Cloak of Shadows
                // Envenom
         32645, // rank 1
         32684, // rank 2
         57992, // rank 3
         57993, // rank 4
         51722, // Dismantle
         51723, // Fan of Knives
         57934, // Tricks of the Trade
     }},
    {CLASS_PRIEST,
     {
         // Power Word: Shield
         17,    // rank 1
         592,   // rank 2
         600,   // rank 3
         3747,  // rank 4
         6065,  // rank 5
         6066,  // rank 6
         10898, // rank 7
         10899, // rank 8
         10900, // rank 9
         10901, // rank 10
         25217, // rank 11
         25218, // rank 12
         48065, // rank 13
         48066, // rank 14
                // Renew
         139,   // rank 1
         6074,  // rank 2
         6075,  // rank 3
         6076,  // rank 4
         6077,  // rank 5
         6078,  // rank 6
         10927, // rank 7
         10928, // rank 8
         10929, // rank 9
         25315, // rank 10
         25221, // rank 11
         25222, // rank 12
         48067, // rank 13
         48068, // rank 14
         453,   // Mind Soothe
              // Dispel Magic
         527, // rank 1
         988, // rank 2
         528, // Cure Disease
         552, // Abolish Disease
              // Smite
         585,   // rank 1
         591,   // rank 2
         598,   // rank 3
         984,   // rank 4
         1004,  // rank 5
         6060,  // rank 6
         10933, // rank 7
         10934, // rank 8
         25363, // rank 9
         25364, // rank 10
         48122, // rank 11
         48123, // rank 12
         586,   // Fade
              // Inner Fire
         588,   // rank 1
         7128,  // rank 2
         602,   // rank 3
         1006,  // rank 4
         10951, // rank 5
         10952, // rank 6
         25431, // rank 7
         48040, // rank 8
         48168, // rank 9
                // Shadow Word: Pain
         589,   // rank 1
         594,   // rank 2
         970,   // rank 3
         992,   // rank 4
         2767,  // rank 5
         10892, // rank 6
         10893, // rank 7
         10894, // rank 8
         25367, // rank 9
         25368, // rank 10
         48124, // rank 11
         48125, // rank 12
                // Prayer of Healing
         596,   // rank 1
         996,   // rank 2
         10960, // rank 3
         10961, // rank 4
         25316, // rank 5
         25308, // rank 6
         48072, // rank 7
         605,   // Mind Control
              // Shadow Protection
         976,   // rank 1
         10957, // rank 2
         10958, // rank 3
         25433, // rank 4
         48169, // rank 5
                // Power Word: Fortitude
         1243,  // rank 1
         1244,  // rank 2
         1245,  // rank 3
         2791,  // rank 4
         10937, // rank 5
         10938, // rank 6
         25389, // rank 7
         48161, // rank 8
         1706,  // Levitate
               // Resurrection
         2006,  // rank 1
         2010,  // rank 2
         10880, // rank 3
         10881, // rank 4
         20770, // rank 5
         25435, // rank 6
         48171, // rank 7
                // Lesser Heal
         2050, // rank 1
         2052, // rank 2
         2053, // rank 3
               // Heal
         2054, // rank 1
         2055, // rank 2
         6063, // rank 3
         6064, // rank 4
               // Greater Heal
         2060,  // rank 1
         10963, // rank 2
         10964, // rank 3
         10965, // rank 4
         25314, // rank 5
         25210, // rank 6
         25213, // rank 7
         48062, // rank 8
         48063, // rank 9
                // Flash Heal
         2061,  // rank 1
         9472,  // rank 2
         9473,  // rank 3
         9474,  // rank 4
         10915, // rank 5
         10916, // rank 6
         10917, // rank 7
         25233, // rank 8
         25235, // rank 9
         48070, // rank 10
         48071, // rank 11
                // Mind Vision
         2096,  // rank 1
         10909, // rank 2
                // Devouring Plague
         2944,  // rank 1
         19276, // rank 2
         19277, // rank 3
         19278, // rank 4
         19279, // rank 5
         19280, // rank 6
         25467, // rank 7
         48299, // rank 8
         48300, // rank 9
         6346,  // Fear Ward
               // Mind Blast
         8092,  // rank 1
         8102,  // rank 2
         8103,  // rank 3
         8104,  // rank 4
         8105,  // rank 5
         8106,  // rank 6
         10945, // rank 7
         10946, // rank 8
         10947, // rank 9
         25372, // rank 10
         25375, // rank 11
         48126, // rank 12
         48127, // rank 13
                // Psychic Scream
         8122,  // rank 1
         8124,  // rank 2
         10888, // rank 3
         10890, // rank 4
         8129,  // Mana Burn
               // Shackle Undead
         9484,  // rank 1
         9485,  // rank 2
         10955, // rank 3
                // Divine Spirit
         14752, // rank 1
         14818, // rank 2
         14819, // rank 3
         27841, // rank 4
         25312, // rank 5
         48073, // rank 6
                // Holy Fire
         14914, // rank 1
         15262, // rank 2
         15263, // rank 3
         15264, // rank 4
         15265, // rank 5
         15266, // rank 6
         15267, // rank 7
         15261, // rank 8
         25384, // rank 9
         48134, // rank 10
         48135, // rank 11
                // Holy Nova
         15237, // rank 1
         15430, // rank 2
         15431, // rank 3
         27799, // rank 4
         27800, // rank 5
         27801, // rank 6
         25331, // rank 7
         48077, // rank 8
         48078, // rank 9
                // Prayer of Fortitude
         21562, // rank 1
         21564, // rank 2
         25392, // rank 3
         48162, // rank 4
                // Prayer of Spirit
         27681, // rank 1
         32999, // rank 2
         48074, // rank 3
                // Prayer of Shadow Protection
         27683, // rank 1
         39374, // rank 2
         48170, // rank 3
         32375, // Mass Dispel
                // Shadow Word: Death
         32379, // rank 1
         32996, // rank 2
         48157, // rank 3
         48158, // rank 4
                // Binding Heal
         32546, // rank 1
         48119, // rank 2
         48120, // rank 3
                // Prayer of Mending
         33076, // rank 1
         48112, // rank 2
         48113, // rank 3
         34433, // Shadowfiend
         34919, // Vampiric Touch
                // Mind Sear
         48045, // rank 1
         53023, // rank 2
         64843, // Divine Hymn
         64901, // Hymn of Hope
     }},
    {CLASS_DEATH_KNIGHT,
     {
         3714,  // Path of Frost
         42650, // Army of the Dead
                // Death and Decay
         43265, // rank 1
         49936, // rank 2
         49937, // rank 3
         49938, // rank 4
                // Plague Strike
         45462, // rank 1
         49917, // rank 2
         49918, // rank 3
         49919, // rank 4
         49920, // rank 5
         49921, // rank 6
                // Icy Touch
         45477, // rank 1
         49896, // rank 2
         49903, // rank 3
         49904, // rank 4
         49909, // rank 5
         45524, // Chains of Ice
         45529, // Blood Tap
                // Blood Strike
         45902, // rank 1
         49926, // rank 2
         49927, // rank 3
         49928, // rank 4
         49929, // rank 5
         49930, // rank 6
         46584, // Raise Dead
         47476, // Strangulate
         47528, // Mind Freeze
                // Death Coil
         47541, // rank 1
         49892, // rank 2
         49893, // rank 3
         49894, // rank 4
         49895, // rank 5
         47568, // Empower Rune Weapon
         48263, // Frost Presence
         48265, // Unholy Presence
         48707, // Anti-Magic Shell
                // Blood Boil
         48721, // rank 1
         49939, // rank 2
         49940, // rank 3
         49941, // rank 4
         48743, // Death Pact
         48778, // Acherus Deathcharger
         48792, // Icebound Fortitude
                // Obliterate
         49020, // rank 1
         51423, // rank 2
         51424, // rank 3
         51425, // rank 4
                // Death Strike
         49998, // rank 1
         49999, // rank 2
         45463, // rank 3
         49923, // rank 4
         49924, // rank 5
         50842, // Pestilence
                // Will of the Necropolis
         52284, // rank 1
         52285, // rank 2
         52286, // rank 3
         53323, // Rune of Swordshattering
         53331, // Rune of Lichbane
         53341, // Rune of Cinderglacier
         53342, // Rune of Spellshattering
         53343, // Rune of Razorice
         53344, // Rune of the Fallen Crusader
         53428, // Runeforging
         54446, // Rune of Swordbreaking
         54447, // Rune of Spellbreaking
         56222, // Dark Command
         56815, // Rune Strike
                // Horn of Winter
         57330, // rank 1
         57623, // rank 2
         61999, // Raise Ally
         62158, // Rune of the Stoneskin Gargoyle
                // Death Coil
         62900, // rank 1
         62901, // rank 2
         62902, // rank 3
         62903, // rank 4
         62904, // rank 5
         70164, // Rune of the Nerubian Carapace
     }},
    {CLASS_SHAMAN,
     {
         131, // Water Breathing
              // Lightning Shield
         324,   // rank 1
         325,   // rank 2
         905,   // rank 3
         945,   // rank 4
         8134,  // rank 5
         10431, // rank 6
         10432, // rank 7
         25469, // rank 8
         25472, // rank 9
         49280, // rank 10
         49281, // rank 11
                // Healing Wave
         331,   // rank 1
         332,   // rank 2
         547,   // rank 3
         913,   // rank 4
         939,   // rank 5
         959,   // rank 6
         8005,  // rank 7
         10395, // rank 8
         10396, // rank 9
         25357, // rank 10
         25391, // rank 11
         25396, // rank 12
         49272, // rank 13
         49273, // rank 14
                // Purge
         370,  // rank 1
         8012, // rank 2
               // Lightning Bolt
         403,   // rank 1
         529,   // rank 2
         548,   // rank 3
         915,   // rank 4
         943,   // rank 5
         6041,  // rank 6
         10391, // rank 7
         10392, // rank 8
         15207, // rank 9
         15208, // rank 10
         25448, // rank 11
         25449, // rank 12
         49237, // rank 13
         49238, // rank 14
                // Chain Lightning
         421,   // rank 1
         930,   // rank 2
         2860,  // rank 3
         10605, // rank 4
         25439, // rank 5
         25442, // rank 6
         49270, // rank 7
         49271, // rank 8
         526,   // Cure Toxins
         546,   // Water Walking
         556,   // Astral Recall
              // Chain Heal
         1064,  // rank 1
         10622, // rank 2
         10623, // rank 3
         25422, // rank 4
         25423, // rank 5
         55458, // rank 6
         55459, // rank 7
                // Fire Nova
         1535,  // rank 1
         8498,  // rank 2
         8499,  // rank 3
         11314, // rank 4
         11315, // rank 5
         25546, // rank 6
         25547, // rank 7
         61649, // rank 8
         61657, // rank 9
                // Ancestral Spirit
         2008,  // rank 1
         20609, // rank 2
         20610, // rank 3
         20776, // rank 4
         20777, // rank 5
         25590, // rank 6
         49277, // rank 7
         2062,  // Earth Elemental Totem
         2484,  // Earthbind Totem
         2645,  // Ghost Wolf
         2825,  // Bloodlust
         2894,  // Fire Elemental Totem
               // Searing Totem
         3599,  // rank 1
         6363,  // rank 2
         6364,  // rank 3
         6365,  // rank 4
         10437, // rank 5
         10438, // rank 6
         25533, // rank 7
         58699, // rank 8
         58703, // rank 9
         58704, // rank 10
         3738,  // Wrath of Air Totem
               // Healing Stream Totem
         5394,  // rank 1
         6375,  // rank 2
         6377,  // rank 3
         10462, // rank 4
         10463, // rank 5
         25567, // rank 6
         58755, // rank 7
         58756, // rank 8
         58757, // rank 9
                // Mana Spring Totem
         5675,  // rank 1
         10495, // rank 2
         10496, // rank 3
         10497, // rank 4
         25570, // rank 5
         58771, // rank 6
         58773, // rank 7
         58774, // rank 8
                // Stoneclaw Totem
         5730,  // rank 1
         6390,  // rank 2
         6391,  // rank 3
         6392,  // rank 4
         10427, // rank 5
         10428, // rank 6
         25525, // rank 7
         58580, // rank 8
         58581, // rank 9
         58582, // rank 10
         6196,  // Far Sight
         6495,  // Sentry Totem
               // Lesser Healing Wave
         8004,  // rank 1
         8008,  // rank 2
         8010,  // rank 3
         10466, // rank 4
         10467, // rank 5
         10468, // rank 6
         25420, // rank 7
         49275, // rank 8
         49276, // rank 9
                // Rockbiter Weapon
         8017,  // rank 1
         8018,  // rank 2
         8019,  // rank 3
         10399, // rank 4
                // Flametongue Weapon
         8024,  // rank 1
         8027,  // rank 2
         8030,  // rank 3
         16339, // rank 4
         16341, // rank 5
         16342, // rank 6
         25489, // rank 7
         58785, // rank 8
         58789, // rank 9
         58790, // rank 10
                // Frostbrand Weapon
         8033,  // rank 1
         8038,  // rank 2
         10456, // rank 3
         16355, // rank 4
         16356, // rank 5
         25500, // rank 6
         58794, // rank 7
         58795, // rank 8
         58796, // rank 9
                // Earth Shock
         8042,  // rank 1
         8044,  // rank 2
         8045,  // rank 3
         8046,  // rank 4
         10412, // rank 5
         10413, // rank 6
         10414, // rank 7
         25454, // rank 8
         49230, // rank 9
         49231, // rank 10
                // Flame Shock
         8050,  // rank 1
         8052,  // rank 2
         8053,  // rank 3
         10447, // rank 4
         10448, // rank 5
         29228, // rank 6
         25457, // rank 7
         49232, // rank 8
         49233, // rank 9
                // Frost Shock
         8056,  // rank 1
         8058,  // rank 2
         10472, // rank 3
         10473, // rank 4
         25464, // rank 5
         49235, // rank 6
         49236, // rank 7
                // Stoneskin Totem
         8071,  // rank 1
         8154,  // rank 2
         8155,  // rank 3
         10406, // rank 4
         10407, // rank 5
         10408, // rank 6
         25508, // rank 7
         25509, // rank 8
         58751, // rank 9
         58753, // rank 10
                // Strength of Earth Totem
         8075,  // rank 1
         8160,  // rank 2
         8161,  // rank 3
         10442, // rank 4
         25361, // rank 5
         25528, // rank 6
         57622, // rank 7
         58643, // rank 8
         8143,  // Tremor Totem
         8170,  // Cleansing Totem
         8177,  // Grounding Totem
               // Frost Resistance Totem
         8181,  // rank 1
         10478, // rank 2
         10479, // rank 3
         25560, // rank 4
         58741, // rank 5
         58745, // rank 6
                // Fire Resistance Totem
         8184,  // rank 1
         10537, // rank 2
         10538, // rank 3
         25563, // rank 4
         58737, // rank 5
         58739, // rank 6
                // Magma Totem
         8190,  // rank 1
         10585, // rank 2
         10586, // rank 3
         10587, // rank 4
         25552, // rank 5
         58731, // rank 6
         58734, // rank 7
                // Flametongue Totem
         8227,  // rank 1
         8249,  // rank 2
         10526, // rank 3
         16387, // rank 4
         25557, // rank 5
         58649, // rank 6
         58652, // rank 7
         58656, // rank 8
                // Windfury Weapon
         8232,  // rank 1
         8235,  // rank 2
         10486, // rank 3
         16362, // rank 4
         25505, // rank 5
         58801, // rank 6
         58803, // rank 7
         58804, // rank 8
         8512,  // Windfury Totem
         8737,  // Mail
               // Nature Resistance Totem
         10595, // rank 1
         10600, // rank 2
         10601, // rank 3
         25574, // rank 4
         58746, // rank 5
         58749, // rank 6
         20608, // Reincarnation
         30824, // Shamanistic Rage
         32182, // Heroism
         36591, // Spirit Weapons
         36936, // Totemic Recall
                // Lava Burst
         51505, // rank 1
         60043, // rank 2
         51514, // Hex
                // Earthliving Weapon
         51730, // rank 1
         51988, // rank 2
         51991, // rank 3
         51992, // rank 4
         51993, // rank 5
         51994, // rank 6
                // Water Shield
         52127, // rank 1
         52129, // rank 2
         52131, // rank 3
         52134, // rank 4
         52136, // rank 5
         52138, // rank 6
         24398, // rank 7
         33736, // rank 8
         57960, // rank 9
         57994, // Wind Shear
         66842, // Call of the Elements
         66843, // Call of the Ancestors
         66844, // Call of the Spirits
     }},
    {CLASS_MAGE,
     {
         // Blizzard
         10,    // rank 1
         6141,  // rank 2
         8427,  // rank 3
         10185, // rank 4
         10186, // rank 5
         10187, // rank 6
         27085, // rank 7
         42939, // rank 8
         42940, // rank 9
         66,    // Invisibility
             // Frostbolt
         116,   // rank 1
         205,   // rank 2
         837,   // rank 3
         7322,  // rank 4
         8406,  // rank 5
         8407,  // rank 6
         8408,  // rank 7
         10179, // rank 8
         10180, // rank 9
         10181, // rank 10
         25304, // rank 11
         27071, // rank 12
         27072, // rank 13
         38697, // rank 14
         42841, // rank 15
         42842, // rank 16
                // Polymorph
         118,   // rank 1
         12824, // rank 2
         12825, // rank 3
         12826, // rank 4
                // Cone of Cold
         120,   // rank 1
         8492,  // rank 2
         10159, // rank 3
         10160, // rank 4
         10161, // rank 5
         27087, // rank 6
         42930, // rank 7
         42931, // rank 8
                // Frost Nova
         122,   // rank 1
         865,   // rank 2
         6131,  // rank 3
         10230, // rank 4
         27088, // rank 5
         42917, // rank 6
         130,   // Slow Fall
              // Fireball
         133,   // rank 1
         143,   // rank 2
         145,   // rank 3
         3140,  // rank 4
         8400,  // rank 5
         8401,  // rank 6
         8402,  // rank 7
         10148, // rank 8
         10149, // rank 9
         10150, // rank 10
         10151, // rank 11
         25306, // rank 12
         27070, // rank 13
         38692, // rank 14
         42832, // rank 15
         42833, // rank 16
                // Frost Armor
         168,  // rank 1
         7300, // rank 2
         7301, // rank 3
         475,  // Remove Curse
              // Fire Ward
         543,   // rank 1
         8457,  // rank 2
         8458,  // rank 3
         10223, // rank 4
         10225, // rank 5
         27128, // rank 6
         43010, // rank 7
                // Conjure Food
         587,   // rank 1
         597,   // rank 2
         990,   // rank 3
         6129,  // rank 4
         10144, // rank 5
         10145, // rank 6
         28612, // rank 7
         33717, // rank 8
                // Dampen Magic
         604,   // rank 1
         8450,  // rank 2
         8451,  // rank 3
         10173, // rank 4
         10174, // rank 5
         33944, // rank 6
         43015, // rank 7
                // Conjure Mana Gem
         759,   // rank 1
         3552,  // rank 2
         10053, // rank 3
         10054, // rank 4
         27101, // rank 5
         42985, // rank 6
                // Amplify Magic
         1008,  // rank 1
         8455,  // rank 2
         10169, // rank 3
         10170, // rank 4
         27130, // rank 5
         33946, // rank 6
         43017, // rank 7
                // Arcane Explosion
         1449,  // rank 1
         8437,  // rank 2
         8438,  // rank 3
         8439,  // rank 4
         10201, // rank 5
         10202, // rank 6
         27080, // rank 7
         27082, // rank 8
         42920, // rank 9
         42921, // rank 10
                // Arcane Intellect
         1459,  // rank 1
         1460,  // rank 2
         1461,  // rank 3
         10156, // rank 4
         10157, // rank 5
         27126, // rank 6
         42995, // rank 7
                // Mana Shield
         1463,  // rank 1
         8494,  // rank 2
         8495,  // rank 3
         10191, // rank 4
         10192, // rank 5
         10193, // rank 6
         27131, // rank 7
         43019, // rank 8
         43020, // rank 9
         1953,  // Blink
               // Flamestrike
         2120,  // rank 1
         2121,  // rank 2
         8422,  // rank 3
         8423,  // rank 4
         10215, // rank 5
         10216, // rank 6
         27086, // rank 7
         42925, // rank 8
         42926, // rank 9
                // Fire Blast
         2136,  // rank 1
         2137,  // rank 2
         2138,  // rank 3
         8412,  // rank 4
         8413,  // rank 5
         10197, // rank 6
         10199, // rank 7
         27078, // rank 8
         27079, // rank 9
         42872, // rank 10
         42873, // rank 11
         2139,  // Counterspell
               // Scorch
         2948,  // rank 1
         8444,  // rank 2
         8445,  // rank 3
         8446,  // rank 4
         10205, // rank 5
         10206, // rank 6
         10207, // rank 7
         27073, // rank 8
         27074, // rank 9
         42858, // rank 10
         42859, // rank 11
                // Arcane Missiles
         5143,  // rank 1
         5144,  // rank 2
         5145,  // rank 3
         8416,  // rank 4
         8417,  // rank 5
         10211, // rank 6
         10212, // rank 7
         25345, // rank 8
         27075, // rank 9
         38699, // rank 10
         38704, // rank 11
         42843, // rank 12
         42846, // rank 13
                // Conjure Water
         5504,  // rank 1
         5505,  // rank 2
         5506,  // rank 3
         6127,  // rank 4
         10138, // rank 5
         10139, // rank 6
         10140, // rank 7
         37420, // rank 8
         27090, // rank 9
                // Mage Armor
         6117,  // rank 1
         22782, // rank 2
         22783, // rank 3
         27125, // rank 4
         43023, // rank 5
         43024, // rank 6
                // Frost Ward
         6143,  // rank 1
         8461,  // rank 2
         8462,  // rank 3
         10177, // rank 4
         28609, // rank 5
         32796, // rank 6
         43012, // rank 7
                // Ice Armor
         7302,  // rank 1
         7320,  // rank 2
         10219, // rank 3
         10220, // rank 4
         27124, // rank 5
         43008, // rank 6
         12051, // Evocation
         12355, // Impact
                // Arcane Brilliance
         23028, // rank 1
         27127, // rank 2
         43002, // rank 3
         30449, // Spellsteal
                // Arcane Blast
         30451, // rank 1
         42894, // rank 2
         42896, // rank 3
         42897, // rank 4
                // Ice Lance
         30455, // rank 1
         42913, // rank 2
         42914, // rank 3
                // Molten Armor
         30482, // rank 1
         43045, // rank 2
         43046, // rank 3
                // Conjure Refreshment
         42955, // rank 1
         42956, // rank 2
                // Ritual of Refreshment
         43987, // rank 1
         58659, // rank 2
                // Frostfire Bolt
         44614, // rank 1
         47610, // rank 2
         45438, // Ice Block
         55342, // Mirror Image
     }},
    {CLASS_WARLOCK,
     {
         126, // Eye of Kilrogg
         132, // Detect Invisibility
              // Corruption
         172,   // rank 1
         6222,  // rank 2
         6223,  // rank 3
         7648,  // rank 4
         11671, // rank 5
         11672, // rank 6
         25311, // rank 7
         27216, // rank 8
         47812, // rank 9
         47813, // rank 10
                // Immolate
         348,   // rank 1
         707,   // rank 2
         1094,  // rank 3
         2941,  // rank 4
         11665, // rank 5
         11667, // rank 6
         11668, // rank 7
         25309, // rank 8
         27215, // rank 9
         47810, // rank 10
         47811, // rank 11
                // Curse of Doom
         603,   // rank 1
         30910, // rank 2
         47867, // rank 3
                // Shadow Bolt
         686,   // rank 1
         695,   // rank 2
         705,   // rank 3
         1088,  // rank 4
         1106,  // rank 5
         7641,  // rank 6
         11659, // rank 7
         11660, // rank 8
         11661, // rank 9
         25307, // rank 10
         27209, // rank 11
         47808, // rank 12
         47809, // rank 13
                // Demon Skin
         687, // rank 1
         696, // rank 2
         688, // Summon Imp
              // Drain Life
         689,   // rank 1
         699,   // rank 2
         709,   // rank 3
         7651,  // rank 4
         11699, // rank 5
         11700, // rank 6
         27219, // rank 7
         27220, // rank 8
         47857, // rank 9
         691,   // Summon Felhunter
              // Create Soulstone
         693,   // rank 1
         20752, // rank 2
         20755, // rank 3
         20756, // rank 4
         20757, // rank 5
         27238, // rank 6
         47884, // rank 7
         697,   // Summon Voidwalker
         698,   // Ritual of Summoning
              // Curse of Weakness
         702,   // rank 1
         1108,  // rank 2
         6205,  // rank 3
         7646,  // rank 4
         11707, // rank 5
         11708, // rank 6
         27224, // rank 7
         30909, // rank 8
         50511, // rank 9
                // Demon Armor
         706,   // rank 1
         1086,  // rank 2
         11733, // rank 3
         11734, // rank 4
         11735, // rank 5
         27260, // rank 6
         47793, // rank 7
         47889, // rank 8
                // Banish
         710,   // rank 1
         18647, // rank 2
         712,   // Summon Succubus
              // Health Funnel
         755,   // rank 1
         3698,  // rank 2
         3699,  // rank 3
         3700,  // rank 4
         11693, // rank 5
         11694, // rank 6
         11695, // rank 7
         27259, // rank 8
         47856, // rank 9
                // Curse of Agony
         980,   // rank 1
         1014,  // rank 2
         6217,  // rank 3
         11711, // rank 4
         11712, // rank 5
         11713, // rank 6
         27218, // rank 7
         47863, // rank 8
         47864, // rank 9
                // Enslave Demon
         1098,  // rank 1
         11725, // rank 2
         11726, // rank 3
         61191, // rank 4
                // Drain Soul
         1120,  // rank 1
         8288,  // rank 2
         8289,  // rank 3
         11675, // rank 4
         27217, // rank 5
         47855, // rank 6
         1122,  // Inferno
               // Life Tap
         1454,  // rank 1
         1455,  // rank 2
         1456,  // rank 3
         11687, // rank 4
         11688, // rank 5
         11689, // rank 6
         27222, // rank 7
         57946, // rank 8
                // Curse of the Elements
         1490,  // rank 1
         11721, // rank 2
         11722, // rank 3
         27228, // rank 4
         47865, // rank 5
         1710,  // Summon Felsteed
               // Curse of Tongues
         1714,  // rank 1
         11719, // rank 2
                // Hellfire
         1949,  // rank 1
         11683, // rank 2
         11684, // rank 3
         27213, // rank 4
         47823, // rank 5
                // Create Spellstone
         2362,  // rank 1
         17727, // rank 2
         17728, // rank 3
         28172, // rank 4
         47886, // rank 5
         47888, // rank 6
         5138,  // Drain Mana
               // Howl of Terror
         5484,  // rank 1
         17928, // rank 2
         5500,  // Sense Demons
               // Searing Pain
         5676,  // rank 1
         17919, // rank 2
         17920, // rank 3
         17921, // rank 4
         17922, // rank 5
         17923, // rank 6
         27210, // rank 7
         30459, // rank 8
         47814, // rank 9
         47815, // rank 10
         5697,  // Unending Breath
               // Rain of Fire
         5740,  // rank 1
         6219,  // rank 2
         11677, // rank 3
         11678, // rank 4
         27212, // rank 5
         47819, // rank 6
         47820, // rank 7
                // Fear
         5782, // rank 1
         6213, // rank 2
         6215, // rank 3
         5784, // Felsteed
               // Create Healthstone
         6201,  // rank 1
         6202,  // rank 2
         5699,  // rank 3
         11729, // rank 4
         11730, // rank 5
         27230, // rank 6
         47871, // rank 7
         47878, // rank 8
                // Shadow Ward
         6229,  // rank 1
         11739, // rank 2
         11740, // rank 3
         28610, // rank 4
         47890, // rank 5
         47891, // rank 6
                // Soul Fire
         6353,  // rank 1
         17924, // rank 2
         27211, // rank 3
         30545, // rank 4
         47824, // rank 5
         47825, // rank 6
                // Create Firestone
         6366,  // rank 1
         17951, // rank 2
         17952, // rank 3
         17953, // rank 4
         27250, // rank 5
         60219, // rank 6
         60220, // rank 7
                // Death Coil
         6789,  // rank 1
         17925, // rank 2
         17926, // rank 3
         27223, // rank 4
         47859, // rank 5
         47860, // rank 6
         23161, // Dreadsteed
                // Seed of Corruption
         27243, // rank 1
         47835, // rank 2
         47836, // rank 3
                // Seed of Corruption
         27285, // rank 1
         47833, // rank 2
         47834, // rank 3
                // Fel Armor
         28176, // rank 1
         28189, // rank 2
         47892, // rank 3
         47893, // rank 4
                // Incinerate
         29722, // rank 1
         32231, // rank 2
         47837, // rank 3
         47838, // rank 4
         29858, // Soulshatter
         29886, // Create Soulwell
                // Ritual of Souls
         29893, // rank 1
         58887, // rank 2
                // Shadowflame
         47897, // rank 1
         61290, // rank 2
         48018, // Demonic Circle: Summon
         48020, // Demonic Circle: Teleport
         58889, // Create Soulwell
     }},
    {CLASS_DRUID,
     {
         // Demoralizing Roar
         99,    // rank 1
         1735,  // rank 2
         9490,  // rank 3
         9747,  // rank 4
         9898,  // rank 5
         26998, // rank 6
         48559, // rank 7
         48560, // rank 8
                // Entangling Roots
         339,   // rank 1
         1062,  // rank 2
         5195,  // rank 3
         5196,  // rank 4
         9852,  // rank 5
         9853,  // rank 6
         26989, // rank 7
         53308, // rank 8
                // Thorns
         467,   // rank 1
         782,   // rank 2
         1075,  // rank 3
         8914,  // rank 4
         9756,  // rank 5
         9910,  // rank 6
         26992, // rank 7
         53307, // rank 8
                // Tranquility
         740,   // rank 1
         8918,  // rank 2
         9862,  // rank 3
         9863,  // rank 4
         26983, // rank 5
         48446, // rank 6
         48447, // rank 7
         768,   // Cat Form
         770,   // Faerie Fire
              // Rejuvenation
         774,   // rank 1
         1058,  // rank 2
         1430,  // rank 3
         2090,  // rank 4
         2091,  // rank 5
         3627,  // rank 6
         8910,  // rank 7
         9839,  // rank 8
         9840,  // rank 9
         9841,  // rank 10
         25299, // rank 11
         26981, // rank 12
         26982, // rank 13
         48440, // rank 14
         48441, // rank 15
                // Swipe (Bear)
         779,   // rank 1
         780,   // rank 2
         769,   // rank 3
         9754,  // rank 4
         9908,  // rank 5
         26997, // rank 6
         48561, // rank 7
         48562, // rank 8
         783,   // Travel Form
         1066,  // Aquatic Form
               // Rip
         1079,  // rank 1
         9492,  // rank 2
         9493,  // rank 3
         9752,  // rank 4
         9894,  // rank 5
         9896,  // rank 6
         27008, // rank 7
         49799, // rank 8
         49800, // rank 9
                // Claw
         1082,  // rank 1
         3029,  // rank 2
         5201,  // rank 3
         9849,  // rank 4
         9850,  // rank 5
         27000, // rank 6
         48569, // rank 7
         48570, // rank 8
                // Mark of the Wild
         1126,  // rank 1
         5232,  // rank 2
         6756,  // rank 3
         5234,  // rank 4
         8907,  // rank 5
         9884,  // rank 6
         9885,  // rank 7
         26990, // rank 8
         48469, // rank 9
                // Rake
         1822,  // rank 1
         1823,  // rank 2
         1824,  // rank 3
         9904,  // rank 4
         27003, // rank 5
         48573, // rank 6
         48574, // rank 7
                // Dash
         1850,  // rank 1
         9821,  // rank 2
         33357, // rank 3
                // Hibernate
         2637,  // rank 1
         18657, // rank 2
         18658, // rank 3
         2782,  // Remove Curse
         2893,  // Abolish Poison
               // Soothe Animal
         2908,  // rank 1
         8955,  // rank 2
         9901,  // rank 3
         26995, // rank 4
                // Starfire
         2912,  // rank 1
         8949,  // rank 2
         8950,  // rank 3
         8951,  // rank 4
         9875,  // rank 5
         9876,  // rank 6
         25298, // rank 7
         26986, // rank 8
         48464, // rank 9
         48465, // rank 10
         3025,  // Cat Form (Passive)
               // Wrath
         5176,  // rank 1
         5177,  // rank 2
         5178,  // rank 3
         5179,  // rank 4
         5180,  // rank 5
         6780,  // rank 6
         8905,  // rank 7
         9912,  // rank 8
         26984, // rank 9
         26985, // rank 10
         48459, // rank 11
         48461, // rank 12
                // Healing Touch
         5185,  // rank 1
         5186,  // rank 2
         5187,  // rank 3
         5188,  // rank 4
         5189,  // rank 5
         6778,  // rank 6
         8903,  // rank 7
         9758,  // rank 8
         9888,  // rank 9
         9889,  // rank 10
         25297, // rank 11
         26978, // rank 12
         26979, // rank 13
         48377, // rank 14
         48378, // rank 15
         5209,  // Challenging Roar
               // Bash
         5211, // rank 1
         6798, // rank 2
         8983, // rank 3
         5215, // Prowl
               // Tiger's Fury
         5217,  // rank 1
         6793,  // rank 2
         9845,  // rank 3
         9846,  // rank 4
         50212, // rank 5
         50213, // rank 6
                // Shred
         5221,  // rank 1
         6800,  // rank 2
         8992,  // rank 3
         9829,  // rank 4
         9830,  // rank 5
         27001, // rank 6
         27002, // rank 7
         48571, // rank 8
         48572, // rank 9
         5225,  // Track Humanoids
         5229,  // Enrage
               // Bear Form
         5487, // rank 1
         9634, // rank 2
               // Ravage
         6785,  // rank 1
         6787,  // rank 2
         9866,  // rank 3
         9867,  // rank 4
         27005, // rank 5
         48578, // rank 6
         48579, // rank 7
         6795,  // Growl
               // Maul
         6807,  // rank 1
         6808,  // rank 2
         6809,  // rank 3
         8972,  // rank 4
         9745,  // rank 5
         9880,  // rank 6
         9881,  // rank 7
         26996, // rank 8
         48479, // rank 9
         48480, // rank 10
                // Moonfire
         8921,  // rank 1
         8924,  // rank 2
         8925,  // rank 3
         8926,  // rank 4
         8927,  // rank 5
         8928,  // rank 6
         8929,  // rank 7
         9833,  // rank 8
         9834,  // rank 9
         9835,  // rank 10
         26987, // rank 11
         26988, // rank 12
         48462, // rank 13
         48463, // rank 14
                // Regrowth
         8936,  // rank 1
         8938,  // rank 2
         8939,  // rank 3
         8940,  // rank 4
         8941,  // rank 5
         9750,  // rank 6
         9856,  // rank 7
         9857,  // rank 8
         9858,  // rank 9
         26980, // rank 10
         48442, // rank 11
         48443, // rank 12
         8946,  // Cure Poison
               // Cower
         8998,  // rank 1
         9000,  // rank 2
         9892,  // rank 3
         31709, // rank 4
         27004, // rank 5
         48575, // rank 6
                // Pounce
         9005,  // rank 1
         9823,  // rank 2
         9827,  // rank 3
         27006, // rank 4
         49803, // rank 5
                // Pounce Bleed
         9007,  // rank 1
         9824,  // rank 2
         9826,  // rank 3
         27007, // rank 4
         49804, // rank 5
                // Nature's Grasp
         16689, // rank 1
         16810, // rank 2
         16811, // rank 3
         16812, // rank 4
         16813, // rank 5
         17329, // rank 6
         27009, // rank 7
         53312, // rank 8
         16857, // Faerie Fire (Feral)
                // Hurricane
         16914, // rank 1
         17401, // rank 2
         17402, // rank 3
         27012, // rank 4
         48467, // rank 5
         18960, // Teleport: Moonglade
                // Rebirth
         20484, // rank 1
         20739, // rank 2
         20742, // rank 3
         20747, // rank 4
         20748, // rank 5
         26994, // rank 6
         48477, // rank 7
         20719, // Feline Grace
         21178, // Bear Form (Passive2)
                // Gift of the Wild
         21849, // rank 1
         21850, // rank 2
         26991, // rank 3
         48470, // rank 4
                // Ferocious Bite
         22568, // rank 1
         22827, // rank 2
         22828, // rank 3
         22829, // rank 4
         31018, // rank 5
         24248, // rank 6
         48576, // rank 7
         48577, // rank 8
                // Maim
         22570, // rank 1
         49802, // rank 2
         22812, // Barkskin
         22842, // Frenzied Regeneration
         29166, // Innervate
                // Lacerate
         33745, // rank 1
         48567, // rank 2
         48568, // rank 3
                // Lifebloom
         33763, // rank 1
         48450, // rank 2
         48451, // rank 3
         33786, // Cyclone
                // Mangle (Cat)
         33876, // rank 1
         33982, // rank 2
         33983, // rank 3
         48565, // rank 4
         48566, // rank 5
                // Mangle (Bear)
         33878, // rank 1
         33986, // rank 2
         33987, // rank 3
         48563, // rank 4
         48564, // rank 5
                // Flight Form
         33943, // rank 1
         40120, // rank 2
         33950, // Flight Form
         50464, // Nourish
                // Revive
         50769, // rank 1
         50768, // rank 2
         50767, // rank 3
         50766, // rank 4
         50765, // rank 5
         50764, // rank 6
         50763, // rank 7
         52610, // Savage Roar
         62078, // Swipe (Cat)
         62600, // Savage Defense
     }},
};

} // namespace

void GrantStartingSpells(Player* player)
{
  auto itr = ClassSpells.find(player->getClass());
  if (itr == ClassSpells.end())
    return;

  for (uint32 spellId : itr->second)
    if (!player->HasSpell(spellId))
      player->addSpell(spellId, SPEC_MASK_ALL, true);
}
} // namespace arenacraft
