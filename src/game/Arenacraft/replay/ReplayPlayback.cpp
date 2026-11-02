#include "ReplayPlayback.hpp"

#include "Arena.h"
#include "ArenaSpectator.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Patrol.hpp"
#include "Player.h"
#include "ReplayConfig.hpp"
#include "ReplayRecorder.hpp"
#include "ReplayTimeline.hpp"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace arenacraft::replay
{
namespace
{
constexpr uint32_t ViewerArrivalTimeoutMs = 60 * IN_MILLISECONDS;
constexpr uint32_t LingerAfterEndMs       = 10 * IN_MILLISECONDS;
constexpr uint32_t ViewerLeaveTimeoutMs   = 30 * IN_MILLISECONDS;
constexpr uint32_t StateIntervalMs        = 1 * IN_MILLISECONDS; // addon STATE message cadence

// Addon messages (ArenaSpectator addon whisper channel): "ACREPLAY\tSTATE;at=..;len=..;paused=..;" / "...END;"
constexpr char const* ReplayAddonPrefix = "ACREPLAY\t";

// viewer -> the playback they watch (world thread only)
std::unordered_map<ObjectGuid, std::weak_ptr<Playback>>& Registry()
{
  static std::unordered_map<ObjectGuid, std::weak_ptr<Playback>> registry;
  return registry;
}

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
  auto playback                 = std::make_shared<Playback>(viewer->GetGUID(), id, std::move(data));
  Registry()[viewer->GetGUID()] = playback;
  static_cast<Arena*>(bg)->StartReplay(std::move(playback));

  viewer->SetPendingSpectatorForBG(bg->GetInstanceID());
  viewer->SetBattlegroundId(bg->GetInstanceID(), bg->GetBgTypeID(), PLAYER_MAX_BATTLEGROUND_QUEUES, false, false,
                            TEAM_NEUTRAL);
  viewer->SetEntryPoint();
  viewer->TeleportTo(mapId, start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), start.GetOrientation(),
                     TELE_TO_GM_MODE);

  // patrol reviews never show the replay id: it would lead to the real names via .replay list
  std::string const title = patrol::IsReviewing(viewer->GetGUID()) ? "Patrol replay" : "Replay #" + std::to_string(id);
  tell(viewer, title + ": loading... Type .replay leave to stop watching.");
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

Playback::Playback(ObjectGuid viewer, uint32_t replayId, ReplayData data)
    : _viewer(viewer), _replayId(replayId), _data(std::move(data))
{
  _objects    = collectObjectGuids(_data.packets);
  _durationMs = _data.packets.empty() ? 0 : _data.packets.back().offsetMs;
}

Playback::~Playback()
{
  // only drop the registry entry if it still points at a playback that is gone (this one)
  if (auto it = Registry().find(_viewer); it != Registry().end() && it->second.expired())
    Registry().erase(it);
}

void Playback::sendPacket(Player* viewer, std::size_t index) const
{
  RecordedPacket const& recorded = _data.packets[index];
  WorldPacket           packet(recorded.opcode, recorded.data.size());
  if (!recorded.data.empty())
  {
    if (isMovementOpcode(recorded.opcode))
    {
      // movement packets carry server timestamps from the recording; rebase them so the client sees the move
      // happen (positionMs - offset) ago
      std::vector<uint8_t> data = recorded.data;
      shiftMovementTime(data, getMSTime() - _data.recordStartMs - _posMs);
      packet.append(data.data(), data.size());
    }
    else
      packet.append(recorded.data.data(), recorded.data.size());
  }
  viewer->GetSession()->SendPacket(&packet);
}

void Playback::sendUpTo(Player* viewer, uint32_t limitMs)
{
  while (_next < _data.packets.size() && _data.packets[_next].offsetMs <= limitMs)
    sendPacket(viewer, _next++);
}

void Playback::destroyRecordedObjects(Player* viewer) const
{
  for (uint64_t guid : _objects)
  {
    WorldPacket data(SMSG_DESTROY_OBJECT, 8 + 1);
    data << uint64(guid);
    data << uint8(0); // not a death animation
    viewer->GetSession()->SendPacket(&data);
  }
}

void Playback::sendState(Player* viewer) const
{
  ArenaSpectator::SendPacketTo(viewer, Acore::StringFormat("{}STATE;at={};len={};paused={};", ReplayAddonPrefix, _posMs,
                                                           _durationMs, _paused ? 1 : 0));
}

void Playback::tickState(Player* viewer, uint32_t diff)
{
  _stateTimer += diff;
  if (_stateTimer < StateIntervalMs)
    return;
  _stateTimer = 0;
  sendState(viewer);
}

bool Playback::setPaused(Player* viewer, bool paused)
{
  if (!controllable())
    return false;
  _paused = paused;
  sendState(viewer);
  return true;
}

bool Playback::seek(Player* viewer, uint32_t targetMs)
{
  if (!controllable())
    return false;

  targetMs = std::min(targetMs, _durationMs);
  if (targetMs < _posMs)
  {
    // the client can't go back: wipe the recorded objects and rebuild the state from the start
    destroyRecordedObjects(viewer);
    _next = 0;
  }

  _posMs           = targetMs;
  std::size_t next = _next;
  for (std::size_t index : planJump(_data.packets, _next, targetMs, next))
    sendPacket(viewer, index);
  _next = next;

  if (_state == State::Finished && _next < _data.packets.size())
    _state = State::Playing;
  _timer = 0;

  patrol::OnReplaySeeked(viewer, _replayId);
  sendState(viewer);
  return true;
}

void Playback::finish(bool watched)
{
  if (_ended)
    return;
  _ended = true;
  patrol::OnReplayEnded(_viewer, _replayId, watched);
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
      _state = State::Playing;
      _timer = 0;
      _posMs = 0;
      tell(viewer,
           (patrol::IsReviewing(_viewer) ? std::string("Patrol replay") : "Replay #" + std::to_string(_replayId)) +
               " started. .replay pause / resume / seek <seconds|m:ss|+N|-N> control it.");
      sendUpTo(viewer, 0);
      _started = true;
      patrol::OnReplayStarted(viewer, _replayId);
      sendState(viewer);
    }
    else if (_timer >= ViewerArrivalTimeoutMs)
    {
      _state = State::Done;
      finish(false);
    }
    break;
  case State::Playing:
    if (!viewer)
    {
      _state = State::Done;
      finish(_started);
      break;
    }
    if (!_paused)
    {
      _posMs = std::min(_posMs + diff, _durationMs);
      sendUpTo(viewer, _posMs);
    }
    tickState(viewer, diff);
    if (_next >= _data.packets.size() && _posMs >= _durationMs)
    {
      _state = State::Finished;
      _timer = 0;
      tell(viewer, "Replay finished. Returning in 10 seconds (.replay pause stays, .replay seek goes back, "
                   ".replay leave exits).");
    }
    break;
  case State::Finished:
    if (!viewer)
    {
      _state = State::Done;
      finish(true);
      break;
    }
    tickState(viewer, diff);
    if (_paused)
      _timer = 0; // a paused viewer stays at the end until they resume or leave
    else if (_timer >= LingerAfterEndMs)
    {
      ArenaSpectator::SendPacketTo(viewer, Acore::StringFormat("{}END;", ReplayAddonPrefix));
      viewer->TeleportToEntryPoint();
      _timer = 0;
      _state = State::Done;
      finish(true);
    }
    break;
  case State::Done:
    // the arena must not go away while the viewer is still on its map
    return viewer && _timer < ViewerLeaveTimeoutMs;
  }
  return true;
}

std::string cannotWatchReason(Player* player) { return whyCannotWatch(player); }

std::shared_ptr<Playback> playbackOf(ObjectGuid viewer)
{
  auto it = Registry().find(viewer);
  return it == Registry().end() ? nullptr : it->second.lock();
}

void watch(Player* viewer, uint32_t id)
{
  if (!config().enabled)
  {
    tell(viewer, "Replay: playback is disabled.");
    return;
  }

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
