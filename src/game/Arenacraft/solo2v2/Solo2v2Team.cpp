#include "Solo2v2Team.hpp"

#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "CharacterCache.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Solo2v2Rules.hpp"

namespace arenacraft::solo2v2
{
namespace
{
uint32 RankFor(ArenaTeam const* team, uint32 rating)
{
  // Mirrors ArenaTeam::FinishGame: 1 + the number of teams of this type with a strictly higher rating.
  uint32 rank = 1;
  for (auto itr = sArenaTeamMgr->GetArenaTeamMapBegin(); itr != sArenaTeamMgr->GetArenaTeamMapEnd(); ++itr)
    if (itr->second != team && itr->second->GetType() == team->GetType() && itr->second->GetStats().Rating > rating)
      ++rank;
  return rank;
}

// The client's PvP pane reads these player fields; ArenaTeam::AddMember only sets the id/type.
void SyncClientFields(Player* player, ArenaTeam const* team, ArenaTeamMember const& member)
{
  uint8 const slot = team->GetSlot();
  player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_WEEK, member.WeekGames);
  player->SetArenaTeamInfoField(slot, ARENA_TEAM_GAMES_SEASON, member.SeasonGames);
  player->SetArenaTeamInfoField(slot, ARENA_TEAM_WINS_SEASON, member.SeasonWins);
  player->SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, member.PersonalRating);
}
} // namespace

ArenaTeam* FindTeam(ObjectGuid guid)
{
  uint32 const teamId = sCharacterCache->GetCharacterArenaTeamIdByGuid(guid, ARENA_SLOT_2v2);
  return teamId ? sArenaTeamMgr->GetArenaTeamById(teamId) : nullptr;
}

ArenaTeam* EnsureTeam(ObjectGuid guid)
{
  if (ArenaTeam* existing = FindTeam(guid))
    return existing;

  std::string name;
  if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
    name = player->GetName();
  else if (!sCharacterCache->GetCharacterNameByGuid(guid, name))
    return nullptr;

  auto* team = new ArenaTeam();
  if (!team->Create(guid, ARENA_TEAM_2v2, teamNameFor(name), 0, 0, 0, 0, 0))
  {
    delete team;
    return nullptr;
  }

  sArenaTeamMgr->AddArenaTeam(team);

  ArenaTeamStats stats = team->GetStats();
  stats.Rating         = static_cast<uint16>(InitialRating);
  stats.Rank           = RankFor(team, stats.Rating);
  team->SetArenaTeamStats(stats);

  if (ArenaTeamMember* member = team->GetMember(guid))
  {
    member->PersonalRating   = static_cast<uint16>(InitialRating);
    member->MatchMakerRating = static_cast<uint16>(InitialMmr);
    member->MaxMMR           = static_cast<uint16>(InitialMmr);

    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
      SyncClientFields(player, team, *member);
  }

  team->SaveToDB(true);
  team->NotifyStatsChanged();
  return team;
}

bool ResetTeam(Player* player)
{
  if (!player)
    return false;

  ArenaTeam* team = FindTeam(player->GetGUID());
  if (!team)
    return false;

  ArenaTeamStats stats = team->GetStats();
  stats.Rating         = static_cast<uint16>(InitialRating);
  stats.WeekGames      = 0;
  stats.WeekWins       = 0;
  stats.SeasonGames    = 0;
  stats.SeasonWins     = 0;
  stats.Rank           = RankFor(team, stats.Rating);

  if (ArenaTeamMember* member = team->GetMember(player->GetGUID()))
  {
    // Keep MatchMakerRating/MaxMMR: only the visible rating is reset.
    member->PersonalRating = static_cast<uint16>(InitialRating);
    member->WeekGames      = 0;
    member->WeekWins       = 0;
    member->SeasonGames    = 0;
    member->SeasonWins     = 0;
    SyncClientFields(player, team, *member);
  }

  team->SetArenaTeamStats(stats);
  team->SaveToDB(true);
  team->NotifyStatsChanged();
  return true;
}

void OnCharacterRenamed(ObjectGuid guid, std::string const& newName)
{
  // The SoloQ 5v5 team is named after the character too.
  if (ArenaTeam* soloq = sArenaTeamMgr->GetArenaTeamByCaptain(guid, ARENA_TEAM_5v5))
  {
    if (ArenaTeamMember* member = soloq->GetMember(guid))
      member->Name = newName;
    soloq->Rename(newName + "'s SoloQ");
  }

  ArenaTeam* team = FindTeam(guid);
  if (!team)
    return;

  if (ArenaTeamMember* member = team->GetMember(guid))
    member->Name = newName;

  team->Rename(teamNameFor(newName));

  if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
    team->Query(player->GetSession());
}
} // namespace arenacraft::solo2v2
