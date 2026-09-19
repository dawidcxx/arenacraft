#include "SoloqArenaQueue.hpp"

#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "DBCStores.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SoloqService.hpp"
#include "WorldPacket.h"

#include <array>
#include <cstddef>

namespace arenacraft::soloq
{
namespace
{
// The queue/badge track stays the (otherwise unused) core 5v5 track, so real
// 3v3 arena teams are untouched. The arena instance spawned for a match is
// created as 3v3, so once players are ported in the match is played, scored and
// has its ready check treated as 3v3.
constexpr BattlegroundQueueTypeId QueueType      = BATTLEGROUND_QUEUE_5v5;
constexpr uint8                   QueueArenaType = ARENA_TYPE_5v5;
constexpr uint8                   MatchArenaType = ARENA_TYPE_3v3;

constexpr std::size_t TeamSize = 3;
} // namespace

bool InArenaQueue(Player* player) { return player && player->InBattlegroundQueueForBattlegroundQueueType(QueueType); }

bool EnterArenaQueue(Player* player)
{
  if (!player || player->InBattleground() || player->InBattlegroundQueue() || !player->HasFreeBattlegroundQueueId())
    return false;

  Battleground* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
  if (!bgTemplate)
    return false;

  PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bgTemplate->GetMapId(), player->GetLevel());
  if (!bracketEntry)
    return false;

  BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(QueueType);

  GroupQueueInfo* ginfo =
      bgQueue.AddGroup(player, nullptr, BATTLEGROUND_AA, bracketEntry, QueueArenaType, true, false, 0, 0);
  if (!ginfo)
    return false;

  uint32 const queueSlot   = player->AddBattlegroundQueueId(QueueType);
  uint32 const avgWaitTime = bgQueue.GetAverageQueueWaitTime(ginfo);

  WorldPacket data;
  sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bgTemplate, queueSlot, STATUS_WAIT_QUEUE, avgWaitTime, 0,
                                                  QueueArenaType, TEAM_NEUTRAL, true);
  player->SendDirectMessage(&data);
  return true;
}

void LeaveArenaQueue(Player* player)
{
  if (!InArenaQueue(player))
    return;

  uint8 const        queueSlot = player->GetBattlegroundQueueIndex(QueueType);
  BattlegroundQueue& bgQueue   = sBattlegroundMgr->GetBattlegroundQueue(QueueType);

  bgQueue.RemovePlayer(player->GetGUID(), true);
  player->RemoveBattlegroundQueueId(QueueType);

  WorldPacket data;
  sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, nullptr, queueSlot, STATUS_NONE, 0, 0, 0, TEAM_NEUTRAL);
  player->SendDirectMessage(&data);
}

bool CreateArenaForMatch(Match const& match)
{
  Battleground* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
  if (!bgTemplate)
    return false;

  PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bgTemplate->GetMapId(), 80);
  if (!bracketEntry)
    return false;

  BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(QueueType);

  std::array<GroupQueueInfo*, 6>           ginfos{};
  std::array<QueuedPlayer const*, 6> const players = playersOf(match);
  for (std::size_t i = 0; i < players.size(); ++i)
  {
    auto const it = bgQueue.m_QueuedPlayers.find(ObjectGuid{players[i]->id});
    if (it == bgQueue.m_QueuedPlayers.end())
      return false;
    ginfos[i] = it->second;
  }

  Battleground* arena = sBattlegroundMgr->CreateNewBattleground(BATTLEGROUND_AA, bracketEntry, MatchArenaType, true);
  if (!arena)
    return false;

  for (std::size_t i = 0; i < TeamSize; ++i)
    bgQueue.InviteGroupToBG(ginfos[i], arena, TEAM_ALLIANCE);
  for (std::size_t i = TeamSize; i < ginfos.size(); ++i)
    bgQueue.InviteGroupToBG(ginfos[i], arena, TEAM_HORDE);

  SoloqService::instance().registerMatch(arena->GetInstanceID(), match);

  arena->StartBattleground();
  return true;
}
} // namespace arenacraft::soloq
