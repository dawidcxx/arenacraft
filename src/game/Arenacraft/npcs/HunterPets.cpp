#include "HunterPets.hpp"

#include "Chat.h"
#include "ItemVendor.hpp"
#include "ObjectMgr.h"
#include "Pet.h"
#include "PetDefines.h"
#include "Player.h"
#include "ScriptedGossip.h"

#include <iterator>
#include <string>

namespace arenacraft
{
namespace
{
// Pet families offered by "Get Pet", each with a tameable Northrend
// representative (verified in creature_template: beast, tameable type flag,
// level ~76-80). Row order is the submenu order.
struct PetFamilyEntry
{
  std::string_view Name;
  uint32           CreatureEntry;
};

constexpr PetFamilyEntry PetFamilies[] = {
    {"Wolf", 29358},   // Frostworg
    {"Cat", 29327},    // Frost Leopard
    {"Spider", 31747}, // Necrotic Webspinner
    {"Bear", 29319},   // Icepaw Bear
    {"Boar", 29996},   // Snorts
    {"Crab", 26521},   // Kili'ua
};

// Creates a level-80 hunter pet of the given family and saves it into the
// given free stable slot. The pet itself is never summoned - it only exists in
// the character_pet table and PetStable, like any other stabled pet.
void GrantPetFamily(Player* player, PetStable& petStable, uint32 familyIndex, uint8 freeSlot)
{
  CreatureTemplate const* cinfo = sObjectMgr->GetCreatureTemplate(PetFamilies[familyIndex].CreatureEntry);
  if (!cinfo || !cinfo->IsTameable(player->CanTameExoticPets()))
    return;

  Pet* pet = new Pet(player, HUNTER_PET);

  bool created = false;
  if (pet->CreateBaseAtCreatureInfo(cinfo, player))
  {
    // Same setup as Unit::InitTamedPet for the tame flow.
    pet->SetCreatorGUID(player->GetGUID());
    pet->SetFaction(player->GetFaction());
    pet->ReplaceAllUnitFlags(UNIT_FLAG_PLAYER_CONTROLLED);

    created = pet->InitStatsForLevel(player->GetLevel());
  }

  if (!created)
  {
    delete pet;
    return;
  }

  pet->GetCharmInfo()->SetPetNumber(sObjectMgr->GeneratePetNumber(), true);
  pet->InitPetCreateSpells(); // family passives + level-up spells (level is already 80)
  pet->SetFullHealth();

  // Pets stay permanently happy, so hand them out fully happy.
  pet->SetMaxPower(POWER_HAPPINESS, pet->GetCreatePowers(POWER_HAPPINESS));
  pet->SetPower(POWER_HAPPINESS, pet->GetMaxPower(POWER_HAPPINESS));

  // SavePetToDB requires the owner guid (UNIT_FIELD_SUMMONEDBY); set directly,
  // not via SetMinion, so the pet never becomes the player's active pet.
  pet->SetOwnerGUID(player->GetGUID());
  pet->InitTalentForLevel(); // talent points for the level (the tame flow does the same)

  pet->SavePetToDB(PetSaveMode(PET_SAVE_FIRST_STABLE_SLOT + freeSlot));
  pet->FillPetInfo(&petStable.StabledPets[freeSlot].emplace());
  petStable.MaxStabledPets = MAX_PET_STABLES;

  delete pet;
}
} // namespace

void SendPetFamilyMenu(Player* player, Creature* creature)
{
  player->PlayerTalkClass->ClearMenus();
  GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();
  menu.SetMenuId(ItemVendor::GossipMenuId);

  uint32 menuIndex = 0;
  for (uint32 i = 0; i < std::size(PetFamilies); ++i, ++menuIndex)
  {
    menu.AddMenuItem(int32(menuIndex), GOSSIP_ICON_INTERACT_1, std::string(PetFamilies[i].Name), 0,
                     PetFamilyActionBase + i, "", 0, false);
    menu.AddGossipMenuItemData(menuIndex, 0, 0);
  }

  SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
}

bool HandleHunterPetAction(Player* player, Creature* creature, uint32 action)
{
  if (action != ItemVendor::UtilGetPet &&
      (action < PetFamilyActionBase || action >= PetFamilyActionBase + std::size(PetFamilies)))
    return false;

  // Pet options only exist for hunters; anything else would try to create a
  // HUNTER_PET for another class.
  if (!player->IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET))
    return true;

  if (action == ItemVendor::UtilGetPet)
  {
    SendPetFamilyMenu(player, creature);
    return true;
  }

  uint32 familyIndex = action - PetFamilyActionBase;

  PetStable& petStable = player->GetOrInitPetStable();

  uint8 freeSlot = MAX_PET_STABLES;
  for (uint8 slot = 0; slot < MAX_PET_STABLES; ++slot)
    if (!petStable.StabledPets[slot])
    {
      freeSlot = slot;
      break;
    }

  if (freeSlot == MAX_PET_STABLES)
  {
    ChatHandler(player->GetSession()).PSendSysMessage("Your stable is full - stable or abandon a pet first.");
    return true;
  }

  GrantPetFamily(player, petStable, familyIndex, freeSlot);

  ChatHandler(player->GetSession()).PSendSysMessage("{} has been added to your stable.", PetFamilies[familyIndex].Name);
  SendPetFamilyMenu(player, creature);
  return true;
}
} // namespace arenacraft
