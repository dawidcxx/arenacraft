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

    // Populate the client's PvP-pane fields too; AddMember only sets id/type.
    uint8 const slot = team->GetSlot();
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_WEEK, member->WeekGames);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_SEASON, member->SeasonGames);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_WINS_SEASON, member->SeasonWins);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, member->PersonalRating);
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

void ApplySoloqResult(Player* player, uint32_t rating, uint32_t mmr, bool won)
{
  if (!player)
    return;

  ArenaTeam* team = FindSoloqTeam(player);
  if (!team)
    return;

  ArenaTeamStats stats = team->GetStats();
  stats.Rating         = static_cast<uint16>(rating);
  stats.WeekGames += 1;
  stats.SeasonGames += 1;
  if (won)
  {
    stats.WeekWins += 1;
    stats.SeasonWins += 1;
  }

  // Mirror ArenaTeam::FinishGame: rank is 1 + the number of teams of this type
  // with a strictly higher rating.
  stats.Rank = 1;
  for (auto itr = sArenaTeamMgr->GetArenaTeamMapBegin(); itr != sArenaTeamMgr->GetArenaTeamMapEnd(); ++itr)
    if (itr->second->GetType() == team->GetType() && itr->second->GetStats().Rating > stats.Rating)
      ++stats.Rank;

  if (ArenaTeamMember* member = team->GetMember(player->GetGUID()))
  {
    member->PersonalRating   = static_cast<uint16>(rating);
    member->MatchMakerRating = static_cast<uint16>(mmr);
    if (member->MatchMakerRating > member->MaxMMR)
      member->MaxMMR = member->MatchMakerRating;

    member->WeekGames += 1;
    member->SeasonGames += 1;
    if (won)
    {
      member->WeekWins += 1;
      member->SeasonWins += 1;
    }

    // The client's PvP pane reads these player fields, so without them the
    // personal rating and record stay stale until the next login.
    uint8 const slot = team->GetSlot();
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_WEEK, member->WeekGames);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_SEASON, member->SeasonGames);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_WINS_SEASON, member->SeasonWins);
    player->SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, member->PersonalRating);
    player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_PERSONAL_RATING, member->PersonalRating,
                                      team->GetType());
  }

  team->SetArenaTeamStats(stats);
  team->SaveToDB(true);
  team->NotifyStatsChanged();
}
} // namespace arenacraft::soloq
