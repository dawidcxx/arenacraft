#include "PreBuffs.hpp"
#include "Battleground.h"
#include "Player.h"

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

void ArenaPreBuffs::OnBattlegroundAddPlayer(Battleground* bg, Player* player)
{
  if (!bg || !bg->isArena() || !player || !player->IsAlive())
    return;

  // Apply every class currently present on the player's team onto the team members already loaded,
  // so the team's composition decides which buffs it enters the prep room with. Re-ran for each
  // player as they land, filling in buffs for teammates that arrived earlier.
  TeamId team = player->GetBgTeamId();
  for (auto const& [providerGuid, provider] : bg->GetPlayers())
  {
    if (!provider || !provider->IsAlive() || provider->GetBgTeamId() != team)
      continue;

    for (ArenaPreBuff const& buff : GetArenaPreBuffsForClass(provider->getClass()))
    {
      if (buff.selfOnly)
      {
        provider->CastSpell(provider, buff.spellId, true);
        continue;
      }

      for (auto const& [targetGuid, target] : bg->GetPlayers())
        if (target && target->IsAlive() && target->GetBgTeamId() == team)
          provider->CastSpell(target, buff.spellId, true);
    }
  }
}
} // namespace arenacraft
