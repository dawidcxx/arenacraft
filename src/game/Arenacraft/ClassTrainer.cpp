#include "ClassTrainer.hpp"

#include "Creature.h"
#include "ItemVendor.hpp"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"

#include <unordered_map>

namespace arenacraft
{
uint32 ClassTrainerEntry(uint8 classId)
{
  // One class trainer per class. These are the entries whose `npc_trainer` data
  // expands to the complete class spell list (other trainers of the same class
  // only carry the low-level ranks). Any of the equal-total entries would do.
  static std::unordered_map<uint8, uint32> const trainers = {
      {CLASS_WARRIOR, 913},        // Lyria Du Lac
      {CLASS_PALADIN, 927},        // Brother Wilhelm
      {CLASS_HUNTER, 987},         // Ogromm
      {CLASS_ROGUE, 917},          // Keryn Sylvius
      {CLASS_PRIEST, 376},         // High Priestess Laurena
      {CLASS_DEATH_KNIGHT, 28471}, // Lady Alistra
      {CLASS_SHAMAN, 986},         // Haromm
      {CLASS_MAGE, 328},           // Zaldimar Wefhellt
      {CLASS_WARLOCK, 461},        // Demisette Cloyce
      {CLASS_DRUID, 3033},         // Turak Runetotem
  };

  auto itr = trainers.find(classId);
  return itr != trainers.end() ? itr->second : 0;
}

TrainerSpellData const* ClassTrainerSpellsFor(Creature const* unit, Player const* player)
{
  if (!unit || !player || unit->GetEntry() != ItemVendor::Entry)
    return nullptr;

  uint32 const entry = ClassTrainerEntry(player->getClass());
  return entry ? sObjectMgr->GetNpcTrainerSpells(entry) : nullptr;
}
} // namespace arenacraft
