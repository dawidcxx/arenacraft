#include "PreBuffs.hpp"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Player.h"

#include <algorithm>

namespace arenacraft
{
std::vector<ArenaPreBuff> const& GetArenaPreBuffsForClass(uint8 playerClass)
{
  static std::vector<ArenaPreBuff> const none = {};

  // Base, non-talented class buffs only. selfOnly = cast on the provider only. Keep the name/rank
  // comment next to each id.
  static std::vector<ArenaPreBuff> const mage = {
      {42995, false}, // Arcane Intellect
      {1008, false},  // Amplify Magic (Rank 1)
      {43024, true},  // Mage Armor (Rank 6) - self only
  };
  static std::vector<ArenaPreBuff> const warrior = {
      {47440, false}, // Commanding Shout (Rank 3)
  };
  static std::vector<ArenaPreBuff> const deathKnight = {
      {57623, false}, // Horn of Winter (Rank 2)
  };
  static std::vector<ArenaPreBuff> const warlock = {
      {132, false},  // Detect Invisibility
      {5697, false}, // Unending Breath
      {47893, true}, // Fel Armor (Rank 4) - self only
  };
  static std::vector<ArenaPreBuff> const priest = {
      {48162, false}, // Prayer of Fortitude (Rank 4)
      {48074, false}, // Prayer of Spirit (Rank 3)
      {48170, false}, // Prayer of Shadow Protection (Rank 3)
  };
  static std::vector<ArenaPreBuff> const druid = {
      {48469, false}, // Mark of the Wild (Rank 9)
      {467, false},   // Thorns (Rank 1)
  };
  static std::vector<ArenaPreBuff> const paladin = {
      {25898, false}, // Greater Blessing of Kings
  };
  static std::vector<ArenaPreBuff> const shaman = {
      {546, false}, // Water Walking
      {131, false}, // Water Breathing
  };

  switch (playerClass)
  {
  case CLASS_MAGE:
    return mage;
  case CLASS_WARRIOR:
    return warrior;
  case CLASS_DEATH_KNIGHT:
    return deathKnight;
  case CLASS_WARLOCK:
    return warlock;
  case CLASS_PRIEST:
    return priest;
  case CLASS_DRUID:
    return druid;
  case CLASS_PALADIN:
    return paladin;
  case CLASS_SHAMAN:
    return shaman;
  default:
    return none;
  }
}

namespace
{
// Applies the buffs a class brings to `target`. selfOnly buffs stay on the class that provides
// them, everything else lands on the target as well. Cast as the target so a teammate that has not
// finished teleporting in can still contribute its buffs.
void ApplyClassBuffsToPlayer(Player* target, uint8 providerClass)
{
  if (!target)
    return;

  for (ArenaPreBuff const& buff : GetArenaPreBuffsForClass(providerClass))
  {
    if (buff.selfOnly && providerClass != target->getClass())
      continue;

    target->CastSpell(target, buff.spellId, true);
  }
}
} // namespace

void ArenaPreBuffs::OnBattlegroundAddPlayer(Battleground* bg, Player* player)
{
  if (!bg || !bg->isArena() || !player || !player->IsAlive())
    return;

  TeamId const team = player->GetBgTeamId();

  // Gather the whole team: the players already in the match plus those still invited (their queue
  // entry disappears once they accept, so neither list alone is complete).
  std::vector<Player*> teamPlayers;
  for (auto const& [guid, present] : bg->GetPlayers())
    if (present && present->IsAlive() && present->GetBgTeamId() == team)
      teamPlayers.push_back(present);

  for (Player* invited : sBattlegroundMgr->GetInvitedPlayers(bg->GetInstanceID(), team))
    if (std::find(teamPlayers.begin(), teamPlayers.end(), invited) == teamPlayers.end())
      teamPlayers.push_back(invited);

  for (Player* member : teamPlayers)
    ApplyClassBuffsToPlayer(player, member->getClass());
}
} // namespace arenacraft
