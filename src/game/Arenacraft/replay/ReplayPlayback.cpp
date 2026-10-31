#include "ReplayPlayback.hpp"

#include "Arena.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "ReplayConfig.hpp"
#include "ReplayRecorder.hpp"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <memory>

namespace arenacraft::replay
{
namespace
{
constexpr uint32_t ViewerArrivalTimeoutMs = 60 * IN_MILLISECONDS;
constexpr uint32_t LingerAfterEndMs       = 10 * IN_MILLISECONDS;
constexpr uint32_t ViewerLeaveTimeoutMs   = 30 * IN_MILLISECONDS;

void tell(Player* player, std::string const& text) { ChatHandler(player->GetSession()).SendSysMessage(text); }

// Same preconditions as `.spect spectate`, minus the aura check: the viewer is
// teleported into an instance and back to where they stood.
std::string whyCannotWatch(Player* player)
{
  if (player->IsSpectator())
    return "You are already spectating.";
  if (player->IsBeingTeleported() || !player->IsInWorld())
    return "Can't use while being teleported.";
  if (!player->FindMap() || player->FindMap()->Instanceable())
    return "Can't use while in an instance, battleground or arena.";
  if (player->GetVehicle())
    return "Can't be on a vehicle.";
  if (player->IsInCombat())
    return "Can't be in combat.";
  if (player->InBattlegroundQueue())
    return "Leave the arena / battleground queue first.";
  if (player->isUsingLfg())
    return "Can't watch while using LFG.";
  if (player->GetGroup())
    return "Leave your group first.";
  if (player->IsInFlight())
    return "Can't be in flight.";
  if (player->IsMounted())
    return "Dismount first.";
  if (!player->IsAlive())
    return "You must be alive.";
  if (player->m_mover != player || !player->m_Controlled.empty())
    return "You must control only yourself.";
  return {};
}

void openReplay(Player* viewer, uint32_t id, uint32_t mapId, uint32_t bgTypeId, ReplayData data)
{
  PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(mapId, viewer->GetLevel());
  if (!bracketEntry)
    bracketEntry = GetBattlegroundBracketByLevel(mapId, DEFAULT_MAX_LEVEL);
  if (!bracketEntry)
  {
    tell(viewer, "Replay: this arena map is not available.");
    return;
  }

  // Unrated and never started: no players are ever invited, the gates stay
  // shut and nobody can queue into it (it is not in the free slot queue).
  Battleground* bg =
      sBattlegroundMgr->CreateNewBattleground(BattlegroundTypeId(bgTypeId), bracketEntry, ARENA_TYPE_2v2, false);
  if (!bg || !bg->isArena() || bg->GetMapId() != mapId)
  {
    delete bg;
    tell(viewer, "Replay: could not open the arena.");
    return;
  }

  Position const start(data.x, data.y, data.z + 0.25f, data.o);
  sBattlegroundMgr->AddBattleground(bg);
  // keeps the empty arena from being deleted until the playback lets go
  bg->IncreaseInvitedCount(TEAM_ALLIANCE);
  static_cast<Arena*>(bg)->StartReplay(std::make_shared<Playback>(viewer->GetGUID(), id, std::move(data)));

  viewer->SetPendingSpectatorForBG(bg->GetInstanceID());
  viewer->SetBattlegroundId(bg->GetInstanceID(), bg->GetBgTypeID(), PLAYER_MAX_BATTLEGROUND_QUEUES, false, false,
                            TEAM_NEUTRAL);
  viewer->SetEntryPoint();
  viewer->TeleportTo(mapId, start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), start.GetOrientation(),
                     TELE_TO_GM_MODE);

  tell(viewer, "Replay #" + std::to_string(id) + ": loading... Type .replay leave to stop watching.");
}
} // namespace

Player* Playback::viewerOnMap(Battleground* bg) const
{
  Player* viewer = ObjectAccessor::FindConnectedPlayer(_viewer);
  if (!viewer || !viewer->IsInWorld() || viewer->IsBeingTeleported() || !bg->GetBgMap() ||
      viewer->FindMap() != bg->GetBgMap())
    return nullptr;
  return viewer;
}

void Playback::sendDue(Player* viewer)
{
  uint32_t const elapsed = getMSTimeDiff(_startMs, getMSTime());
  // movement packets carry server timestamps from the recording
  uint32_t const timeShift = _startMs - _data.recordStartMs;

  WorldSession* session = viewer->GetSession();
  while (_next < _data.packets.size() && _data.packets[_next].offsetMs <= elapsed)
  {
    RecordedPacket& recorded = _data.packets[_next++];
    if (isMovementOpcode(recorded.opcode))
      shiftMovementTime(recorded.data, timeShift);

    WorldPacket packet(recorded.opcode, recorded.data.size());
    if (!recorded.data.empty())
      packet.append(recorded.data.data(), recorded.data.size());
    session->SendPacket(&packet);

    // sent packets are never needed again
    std::vector<uint8_t>().swap(recorded.data);
  }
}

bool Playback::update(Battleground* bg, uint32_t diff)
{
  _timer += diff;
  Player* viewer = viewerOnMap(bg);

  switch (_state)
  {
  case State::WaitingForViewer:
    if (viewer)
    {
      _state   = State::Playing;
      _timer   = 0;
      _startMs = getMSTime();
      tell(viewer, "Replay #" + std::to_string(_replayId) + " started.");
      sendDue(viewer);
    }
    else if (_timer >= ViewerArrivalTimeoutMs)
      _state = State::Done;
    break;
  case State::Playing:
    if (!viewer)
    {
      _state = State::Done;
      break;
    }
    sendDue(viewer);
    if (_next >= _data.packets.size())
    {
      _state = State::Finished;
      _timer = 0;
      tell(viewer, "Replay finished. Returning in 10 seconds (or type .replay leave).");
    }
    break;
  case State::Finished:
    if (!viewer)
      _state = State::Done;
    else if (_timer >= LingerAfterEndMs)
    {
      viewer->TeleportToEntryPoint();
      _timer = 0;
      _state = State::Done;
    }
    break;
  case State::Done:
    // the arena must not go away while the viewer is still on its map
    return viewer && _timer < ViewerLeaveTimeoutMs;
  }
  return true;
}

void watch(Player* viewer, uint32_t id)
{
  if (std::string const problem = whyCannotWatch(viewer); !problem.empty())
  {
    tell(viewer, "Replay: " + problem);
    return;
  }

  CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ARENA_REPLAY);
  stmt->SetData(0, viewer->GetGUID().GetCounter());
  stmt->SetData(1, id);

  WorldSession*    session = viewer->GetSession();
  ObjectGuid const guid    = viewer->GetGUID();
  session->GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(stmt).WithPreparedCallback(
      [session, guid, id](PreparedQueryResult result)
      {
        Player* player = session->GetPlayer();
        if (!player || player->GetGUID() != guid)
          return;

        if (!result)
        {
          tell(player, "Replay #" + std::to_string(id) + " does not exist (or was already deleted).");
          return;
        }

        Field* fields = result->Fetch();
        if (fields[3].Get<uint64>() > 0)
        {
          // The client can't show a second copy of its own character.
          tell(player, "Replay: you played in this match. Watch it from another character.");
          return;
        }
        if (std::string const problem = whyCannotWatch(player); !problem.empty())
        {
          tell(player, "Replay: " + problem);
          return;
        }

        std::optional<ReplayData> data = decode(fields[2].Get<Binary>());
        if (!data)
        {
          LOG_ERROR("bg.arena", "Arena replay #{} is corrupt", id);
          tell(player, "Replay: this recording is corrupt.");
          return;
        }

        openReplay(player, id, fields[0].Get<uint32>(), fields[1].Get<uint32>(), std::move(*data));
      }));
}
} // namespace arenacraft::replay
