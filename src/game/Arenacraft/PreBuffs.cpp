#include "PreBuffs.hpp"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Pet.h"
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
  static std::vector<ArenaPreBuff> const warrior     = {};
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
  static std::vector<ArenaPreBuff> const paladin = {};
  static std::vector<ArenaPreBuff> const shaman  = {
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

// Gives a pet the non-self-only buffs a class brings. AddAura rather than a cast: the buff lands on the pet only and
// does not depend on the pet being able to cast (party-wide Prayer spells, pet spell checks).
void ApplyClassBuffsToPet(Pet* pet, uint8 providerClass)
{
  if (!pet || !pet->IsAlive())
    return;

  for (ArenaPreBuff const& buff : GetArenaPreBuffsForClass(providerClass))
    if (!buff.selfOnly)
      pet->AddAura(buff.spellId, pet);
}

// The whole team: the players already in the match plus those still invited. A player who accepted but is still on
// the loading screen is in neither list - OnBattlegroundAddPlayer covers them when they land.
std::vector<Player*> CollectArenaTeam(Battleground* bg, TeamId team)
{
  std::vector<Player*> teamPlayers;
  for (auto const& [guid, present] : bg->GetPlayers())
    if (present && present->IsAlive() && present->GetBgTeamId() == team)
      teamPlayers.push_back(present);

  for (Player* invited : sBattlegroundMgr->GetInvitedPlayers(bg->GetInstanceID(), team))
    if (std::find(teamPlayers.begin(), teamPlayers.end(), invited) == teamPlayers.end())
      teamPlayers.push_back(invited);

  return teamPlayers;
}
} // namespace

void ArenaPreBuffs::OnBattlegroundAddPlayer(Battleground* bg, Player* player)
{
  if (!bg || !bg->isArena() || !player || !player->IsAlive())
    return;

  TeamId const team = player->GetBgTeamId();

  // The newcomer gets the buffs of every class on the team (already inside or still invited)...
  for (Player* member : CollectArenaTeam(bg, team))
    ApplyClassBuffsToPlayer(player, member->getClass());

  // ...and brings its own class's buffs to the teammates already inside and their pets. Those loaded in while this
  // player was on the loading screen (accepted, so no longer invited, but not added yet) and never got them: with
  // uneven loading times part of the team went into the match without e.g. Fortitude or Mark of the Wild.
  if (bg->GetStatus() != STATUS_WAIT_JOIN)
    return;
  for (auto const& [guid, present] : bg->GetPlayers())
  {
    if (!present || present == player || !present->IsAlive() || present->GetBgTeamId() != team)
      continue;
    ApplyClassBuffsToPlayer(present, player->getClass());
    ApplyClassBuffsToPet(present->GetPet(), player->getClass());
  }
}

void ApplyArenaPreBuffsToPet(Pet* pet)
{
  if (!pet || !pet->IsAlive())
    return;

  Player* owner = pet->GetOwner();
  if (!owner)
    return;

  // Preparation phase only: a pet summoned or resummoned mid-match does not get free buffs.
  Battleground* bg = owner->GetBattleground();
  if (!bg || !bg->isArena() || bg->GetStatus() != STATUS_WAIT_JOIN)
    return;

  for (Player* member : CollectArenaTeam(bg, owner->GetBgTeamId()))
    ApplyClassBuffsToPet(pet, member->getClass());
}
} // namespace arenacraft
