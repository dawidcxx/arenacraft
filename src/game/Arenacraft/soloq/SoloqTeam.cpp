#include "SoloqTeam.hpp"

#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Types.hpp"

#include <string>

namespace arenacraft::soloq
{
ArenaTeam* FindSoloqTeam(Player* player)
{
  if (!player)
    return nullptr;

  return sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_5v5);
}

bool CreateSoloqTeam(Player* player)
{
  if (!player || player->GetArenaTeamId(ARENA_SLOT_5v5) != 0)
    return false;

  ArenaTeam*        team = new ArenaTeam();
  std::string const name = std::string(player->GetName()) + "'s SoloQ";
  if (!team->Create(player->GetGUID(), ARENA_TYPE_5v5, name, 0, 0, 0, 0, 0))
  {
    delete team;
    return false;
  }

  sArenaTeamMgr->AddArenaTeam(team);

  ArenaTeamStats stats = team->GetStats();
  stats.Rating         = static_cast<uint16>(tuning::InitialRating);
  team->SetArenaTeamStats(stats);

  if (ArenaTeamMember* member = team->GetMember(player->GetGUID()))
  {
    member->PersonalRating   = static_cast<uint16>(tuning::InitialRating);
    member->MatchMakerRating = static_cast<uint16>(tuning::InitialMmr);
  }

  team->SaveToDB(true);
  team->NotifyStatsChanged();
  return true;
}

bool DeleteSoloqTeam(Player* player)
{
  ArenaTeam* team = FindSoloqTeam(player);
  if (!team)
    return false;

  team->Disband();
  delete team;
  return true;
}

std::optional<SoloqTeamInfo> GetSoloqTeamInfo(Player* player)
{
  ArenaTeam* team = FindSoloqTeam(player);
  if (!team)
    return std::nullopt;

  ArenaTeamMember* member = team->GetMember(player->GetGUID());
  return SoloqTeamInfo{team->GetRating(), member ? member->MatchMakerRating : 0u};
}
} // namespace arenacraft::soloq
