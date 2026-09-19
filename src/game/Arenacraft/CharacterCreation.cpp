#include "CharacterCreation.hpp"

#include "PetDefines.h"
#include "Player.h"
#include "SharedDefines.h"

#include <unordered_map>
#include <vector>

namespace arenacraft
{
namespace
{
// Weapon proficiency passives, by class. Only proficiencies the class is meant
// to have are listed (a mage gets daggers/staves/wands/swords, not axes...).
enum WeaponProficiency : uint32
{
  BLOCK        = 107,
  BOWS         = 264,
  CROSSBOWS    = 5011,
  DAGGERS      = 1180,
  FIST_WEAPONS = 15590,
  GUNS         = 266,
  ONE_H_AXES   = 196,
  ONE_H_MACES  = 198,
  ONE_H_SWORDS = 201,
  POLEARMS     = 200,
  SHOOT        = 5019,
  STAVES       = 227,
  TWO_H_AXES   = 197,
  TWO_H_MACES  = 199,
  TWO_H_SWORDS = 202,
  WANDS        = 5009,
  THROW_WAR    = 2567,
};

// Armor proficiency passives, by class. The lower tiers a class can wear
// (cloth/leather/mail) already come from `playercreateinfo_skills`; the ones
// below are the top tiers that normally need a trainer (plate for
// warrior/paladin/death knight, mail for hunter/shaman). Learned up front so an
// instant-80 character can equip the vendor gear immediately.
enum ArmorProficiency : uint32
{
  PLATE_MAIL = 750,
  MAIL       = 8737,
};

std::unordered_map<uint8, uint32> const ClassArmorProficiencies = {
    {CLASS_WARRIOR, PLATE_MAIL},      {CLASS_PALADIN, PLATE_MAIL}, {CLASS_HUNTER, MAIL},
    {CLASS_DEATH_KNIGHT, PLATE_MAIL}, {CLASS_SHAMAN, MAIL},
};

std::unordered_map<uint8, std::vector<uint32>> const ClassWeaponProficiencies = {
    {CLASS_WARRIOR,
     {THROW_WAR, TWO_H_SWORDS, TWO_H_MACES, TWO_H_AXES, STAVES, POLEARMS, ONE_H_SWORDS, ONE_H_MACES, ONE_H_AXES, GUNS,
      FIST_WEAPONS, DAGGERS, CROSSBOWS, BOWS, BLOCK}},
    {CLASS_PRIEST, {WANDS, STAVES, SHOOT, ONE_H_MACES, DAGGERS}},
    {CLASS_PALADIN, {TWO_H_SWORDS, TWO_H_MACES, TWO_H_AXES, POLEARMS, ONE_H_SWORDS, ONE_H_MACES, ONE_H_AXES, BLOCK}},
    {CLASS_ROGUE, {ONE_H_SWORDS, ONE_H_MACES, ONE_H_AXES, GUNS, FIST_WEAPONS, DAGGERS, CROSSBOWS, BOWS}},
    {CLASS_DEATH_KNIGHT, {TWO_H_SWORDS, TWO_H_MACES, TWO_H_AXES, POLEARMS, ONE_H_SWORDS, ONE_H_MACES, ONE_H_AXES}},
    {CLASS_MAGE, {WANDS, STAVES, SHOOT, ONE_H_SWORDS, DAGGERS}},
    {CLASS_SHAMAN, {TWO_H_MACES, TWO_H_AXES, STAVES, ONE_H_MACES, ONE_H_AXES, FIST_WEAPONS, DAGGERS, BLOCK}},
    {CLASS_HUNTER,
     {THROW_WAR, TWO_H_SWORDS, TWO_H_AXES, STAVES, POLEARMS, ONE_H_SWORDS, ONE_H_AXES, GUNS, FIST_WEAPONS, DAGGERS,
      CROSSBOWS, BOWS}},
    {CLASS_DRUID, {TWO_H_MACES, STAVES, POLEARMS, ONE_H_MACES, FIST_WEAPONS, DAGGERS}},
    {CLASS_WARLOCK, {WANDS, STAVES, SHOOT, ONE_H_SWORDS, DAGGERS}},
};

// Riding ranks (skill 762, ascending) + the Northrend flying unlock, plus a
// ground mount. Learned as spells so nothing goes into the inventory.
constexpr uint32 RidingSpells[]     = {33388, 33389, 34090, 34091, 54197};
constexpr uint32 StartingMountSpell = 65917; // Magic Rooster
} // namespace

void GrantStartingArmorProficiencies(Player* player)
{
  auto itr = ClassArmorProficiencies.find(player->getClass());
  if (itr == ClassArmorProficiencies.end())
    return;

  if (!player->HasSpell(itr->second))
    player->addSpell(itr->second, SPEC_MASK_ALL, true);
}

void GrantStartingWeaponSkills(Player* player)
{
  auto itr = ClassWeaponProficiencies.find(player->getClass());
  if (itr == ClassWeaponProficiencies.end())
    return;

  for (uint32 spell : itr->second)
    if (!player->HasSpell(spell))
      player->addSpell(spell, SPEC_MASK_ALL, true);

  player->UpdateSkillsToMaxSkillsForLevel();
}

void GrantStartingMount(Player* player)
{
  for (uint32 spell : RidingSpells)
    if (!player->HasSpell(spell))
      player->addSpell(spell, SPEC_MASK_ALL, true);

  if (!player->HasSpell(StartingMountSpell))
    player->addSpell(StartingMountSpell, SPEC_MASK_ALL, true);
}

void GrantStartingFirstAid(Player* player)
{
  // SetSkill adds the skill line if missing and learns the rewarded bandage
  // ranks.
  player->SetSkill(SKILL_FIRST_AID, 0, 450, 450);
}

void GrantStartingTotems(Player* player)
{
  if (player->getClass() != CLASS_SHAMAN)
    return;

  static constexpr uint32 Totems[] = {5175, 5176, 5177, 5178}; // Earth/Fire/Water/Air
  for (uint32 totem : Totems)
    player->StoreNewItemInBestSlots(totem, 1);
}

void GrantStartingStableSlots(Player* player)
{
  if (!player->IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET))
    return;

  // MaxStabledPets is persisted in characters.stableSlots and restored by
  // Player::_LoadPetStable, so this survives relogs.
  player->GetOrInitPetStable().MaxStabledPets = MAX_PET_STABLES;
}
} // namespace arenacraft
