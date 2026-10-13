#pragma once

#include "AllBattlegroundScript.h"
#include "Define.h"
#include <vector>

class Battleground;
class Pet;
class Player;

namespace arenacraft
{
// A base buff a class brings to its arena team. selfOnly means it is cast on the providing player
// only (e.g. armor buffs), otherwise it is cast on every team member.
struct ArenaPreBuff
{
  uint32 spellId;
  bool   selfOnly;
};

// Returns the curated, non-talented buffs a class brings to its arena team.
std::vector<ArenaPreBuff> const& GetArenaPreBuffsForClass(uint8 playerClass);

// Gives a pet that is (re)summoned during the arena preparation phase the non-self-only buffs of
// its owner's team. Pets are unsummoned on arena entry and come back (and lose non-arena auras)
// only after the player has been added, so OnBattlegroundAddPlayer cannot buff them.
void ApplyArenaPreBuffsToPet(Pet* pet);

// As each player loads into an arena (prep phase), every class currently present on their team
// casts its curated buffs onto the team members already there. Re-running as more players land
// means the full team ends up with the union of its composition's buffs.
class ArenaPreBuffs : public AllBattlegroundScript
{
public:
  ArenaPreBuffs() : AllBattlegroundScript("arenacraft::ArenaPreBuffs", {ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER})
  {
  }

  void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override;
};
} // namespace arenacraft
