#include "ReplayRecorder.hpp"

#include "Battleground.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Timer.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace arenacraft::replay
{
namespace
{
// Hard cap per match so a stuck arena can't eat the server's memory. A busy
// 3v3 produces a few MB per 10 minutes, so this is far above normal.
constexpr std::size_t MaxRecordingBytes = 128u * 1024u * 1024u;
// Matches shorter than this (cancelled at the gates, instant leaves) are not
// worth keeping.
constexpr uint32_t MinDurationMs = 10 * IN_MILLISECONDS;

void setTap(uint32_t guidLow, std::shared_ptr<Recording> recording, bool primary)
{
  if (!guidLow)
    return;
  if (Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guidLow)))
    player->GetSession()->SetReplayTap(std::move(recording), primary);
}

void appendAuras(Recording& recording, Unit* unit)
{
  Unit::VisibleAuraMap const* auras = unit->GetVisibleAuras();
  if (auras->empty())
    return;

  WorldPacket data(SMSG_AURA_UPDATE_ALL);
  data << unit->GetPackGUID();
  for (auto const& [slot, app] : *auras)
    app->BuildUpdatePacket(data, false);
  recording.addInitial(data.GetOpcode(), data.contents(), data.size());
}
} // namespace

bool isMovementOpcode(uint16_t opcode)
{
  switch (opcode)
  {
  case MSG_MOVE_START_FORWARD:
  case MSG_MOVE_START_BACKWARD:
  case MSG_MOVE_STOP:
  case MSG_MOVE_START_STRAFE_LEFT:
  case MSG_MOVE_START_STRAFE_RIGHT:
  case MSG_MOVE_STOP_STRAFE:
  case MSG_MOVE_JUMP:
  case MSG_MOVE_START_TURN_LEFT:
  case MSG_MOVE_START_TURN_RIGHT:
  case MSG_MOVE_STOP_TURN:
  case MSG_MOVE_START_PITCH_UP:
  case MSG_MOVE_START_PITCH_DOWN:
  case MSG_MOVE_STOP_PITCH:
  case MSG_MOVE_SET_RUN_MODE:
  case MSG_MOVE_SET_WALK_MODE:
  case MSG_MOVE_FALL_LAND:
  case MSG_MOVE_START_SWIM:
  case MSG_MOVE_STOP_SWIM:
  case MSG_MOVE_SET_FACING:
  case MSG_MOVE_SET_PITCH:
  case MSG_MOVE_HEARTBEAT:
  case MSG_MOVE_START_ASCEND:
  case MSG_MOVE_STOP_ASCEND:
  case MSG_MOVE_START_DESCEND:
  case MSG_MOVE_TELEPORT:
  case MSG_MOVE_KNOCK_BACK:
  case MSG_MOVE_ROOT:
  case MSG_MOVE_UNROOT:
  case MSG_MOVE_WATER_WALK:
  case MSG_MOVE_FEATHER_FALL:
  case MSG_MOVE_HOVER:
  case MSG_MOVE_SET_RUN_SPEED:
  case MSG_MOVE_SET_RUN_BACK_SPEED:
  case MSG_MOVE_SET_WALK_SPEED:
  case MSG_MOVE_SET_SWIM_SPEED:
  case MSG_MOVE_SET_SWIM_BACK_SPEED:
  case MSG_MOVE_SET_TURN_RATE:
  case MSG_MOVE_SET_FLIGHT_SPEED:
  case MSG_MOVE_SET_FLIGHT_BACK_SPEED:
  case MSG_MOVE_SET_PITCH_RATE:
  case MSG_MOVE_UPDATE_CAN_FLY:
  case MSG_MOVE_GRAVITY_CHNG:
    return true;
  default:
    return false;
  }
}

bool isObserverOpcode(uint16_t opcode)
{
  if (isMovementOpcode(opcode))
    return true;

  switch (opcode)
  {
  // objects
  case SMSG_UPDATE_OBJECT:
  case SMSG_COMPRESSED_UPDATE_OBJECT:
  case SMSG_DESTROY_OBJECT:
  // server driven movement of others (charges, knockbacks, fears, ...)
  case SMSG_MONSTER_MOVE:
  case SMSG_MONSTER_MOVE_TRANSPORT:
  case SMSG_COMPRESSED_MOVES:
  case SMSG_SPLINE_MOVE_FEATHER_FALL:
  case SMSG_SPLINE_MOVE_GRAVITY_DISABLE:
  case SMSG_SPLINE_MOVE_GRAVITY_ENABLE:
  case SMSG_SPLINE_MOVE_LAND_WALK:
  case SMSG_SPLINE_MOVE_NORMAL_FALL:
  case SMSG_SPLINE_MOVE_ROOT:
  case SMSG_SPLINE_MOVE_SET_FLYING:
  case SMSG_SPLINE_MOVE_SET_HOVER:
  case SMSG_SPLINE_MOVE_SET_RUN_MODE:
  case SMSG_SPLINE_MOVE_SET_WALK_MODE:
  case SMSG_SPLINE_MOVE_START_SWIM:
  case SMSG_SPLINE_MOVE_STOP_SWIM:
  case SMSG_SPLINE_MOVE_UNROOT:
  case SMSG_SPLINE_MOVE_UNSET_FLYING:
  case SMSG_SPLINE_MOVE_UNSET_HOVER:
  case SMSG_SPLINE_MOVE_WATER_WALK:
  case SMSG_SPLINE_SET_FLIGHT_BACK_SPEED:
  case SMSG_SPLINE_SET_FLIGHT_SPEED:
  case SMSG_SPLINE_SET_PITCH_RATE:
  case SMSG_SPLINE_SET_RUN_BACK_SPEED:
  case SMSG_SPLINE_SET_RUN_SPEED:
  case SMSG_SPLINE_SET_SWIM_BACK_SPEED:
  case SMSG_SPLINE_SET_SWIM_SPEED:
  case SMSG_SPLINE_SET_TURN_RATE:
  case SMSG_SPLINE_SET_WALK_SPEED:
  // spells, auras, combat
  case SMSG_SPELL_START:
  case SMSG_SPELL_GO:
  case SMSG_SPELL_FAILURE:
  case SMSG_SPELL_FAILED_OTHER:
  case SMSG_SPELL_DELAYED:
  case SMSG_SPELL_UPDATE_CHAIN_TARGETS:
  case MSG_CHANNEL_START:
  case MSG_CHANNEL_UPDATE:
  case SMSG_AURA_UPDATE:
  case SMSG_AURA_UPDATE_ALL:
  case SMSG_SPELLNONMELEEDAMAGELOG:
  case SMSG_SPELLHEALLOG:
  case SMSG_SPELLENERGIZELOG:
  case SMSG_PERIODICAURALOG:
  case SMSG_SPELLLOGMISS:
  case SMSG_SPELLLOGEXECUTE:
  case SMSG_SPELLDISPELLOG:
  case SMSG_SPELLSTEALLOG:
  case SMSG_SPELLINSTAKILLLOG:
  case SMSG_SPELLDAMAGESHIELD:
  case SMSG_SPELLORDAMAGE_IMMUNE:
  case SMSG_SPELLBREAKLOG:
  case SMSG_PROCRESIST:
  case SMSG_RESISTLOG:
  case SMSG_DISPEL_FAILED:
  case SMSG_ATTACKSTART:
  case SMSG_ATTACKSTOP:
  case SMSG_ATTACKERSTATEUPDATE:
  case SMSG_PARTYKILLLOG:
  case SMSG_HEALTH_UPDATE:
  case SMSG_POWER_UPDATE:
  // visuals
  case SMSG_EMOTE:
  case SMSG_PLAY_SPELL_VISUAL:
  case SMSG_PLAY_SPELL_IMPACT:
  case SMSG_GAMEOBJECT_CUSTOM_ANIM:
  case SMSG_GAMEOBJECT_DESPAWN_ANIM:
  // arena frame (players alive per team)
  case SMSG_UPDATE_WORLD_STATE:
    return true;
  default:
    return false;
  }
}

void Recording::onPacket(bool fromPrimary, uint16_t opcode, uint8_t const* data, std::size_t size)
{
  if (fromPrimary)
  {
    if (!isObserverOpcode(opcode))
      return;
  }
  else
  {
    // the primary's own movement, as everyone else sees it
    if (!isMovementOpcode(opcode) || !data)
      return;
    std::vector<uint8_t> const head(data, data + std::min<std::size_t>(size, 9));
    std::size_t                pos = 0;
    if (readPackedGuid(head, pos) != _primaryGuid)
      return;
  }

  append(getMSTimeDiff(_startMs, getMSTime()), opcode, data, size);
}

void Recording::addInitial(uint16_t opcode, uint8_t const* data, std::size_t size) { append(0, opcode, data, size); }

void Recording::append(uint32_t offset, uint16_t opcode, uint8_t const* data, std::size_t size)
{
  std::lock_guard<std::mutex> guard(_lock);
  if (_finished || _bytes + size > MaxRecordingBytes)
    return;

  RecordedPacket& p = _packets.emplace_back();
  p.offsetMs        = offset;
  p.opcode          = opcode;
  if (data && size)
    p.data.assign(data, data + size);
  _bytes += size + 10;
}

std::vector<RecordedPacket> Recording::finish()
{
  std::lock_guard<std::mutex> guard(_lock);
  _finished = true;
  return std::move(_packets);
}

ReplayRecorder& ReplayRecorder::instance()
{
  static ReplayRecorder recorder;
  return recorder;
}

void ReplayRecorder::init()
{
  if (QueryResult result = CharacterDatabase.Query("SELECT MAX(id) FROM arena_replay"))
    _lastId = (*result)[0].Get<uint32>();

  if (uint32 days = config().keepDays)
    CharacterDatabase.Execute("DELETE FROM arena_replay WHERE created_at < NOW() - INTERVAL {} DAY", days);
}

Bracket ReplayRecorder::bracketOf(Battleground const* bg)
{
  if (!bg->isRated())
    return Bracket::Skirmish;
  if (bg->IsSoloq())
    return Bracket::Soloq;
  switch (bg->GetArenaType())
  {
  case ARENA_TYPE_2v2:
    return Bracket::Rated2v2;
  case ARENA_TYPE_3v3:
    return Bracket::Rated3v3;
  default:
    return Bracket::Rated5v5;
  }
}

void ReplayRecorder::onGatesOpen(Battleground* bg)
{
  Config const& cfg = config();
  if (!cfg.enabled || !bg || !bg->isArena())
    return;

  Bracket const bracket = bracketOf(bg);
  if (!cfg.records(bracket))
    return;

  Player* primary   = nullptr;
  Player* secondary = nullptr;
  for (auto const& [guid, player] : bg->GetPlayers())
  {
    if (!player || !player->IsInWorld() || !player->GetSession())
      continue;
    if (player->GetBgTeamId() == TEAM_ALLIANCE && !primary)
      primary = player;
    else if (player->GetBgTeamId() == TEAM_HORDE && !secondary)
      secondary = player;
  }
  if (!primary || !secondary)
    return;

  auto recording = std::make_shared<Recording>(getMSTime(), primary->GetGUID().GetRawValue());

  // What the primary's client already knows (its team, pets, the arena's
  // objects) plus the primary itself as an outsider sees it. The enemies show
  // up in the stream right after: opening the gates widens visibility.
  UpdateData         data;
  std::vector<Unit*> units;
  for (ObjectGuid const& guid : primary->m_clientGUIDs)
  {
    WorldObject* object = ObjectAccessor::GetWorldObject(*primary, guid);
    if (!object)
      continue;
    object->BuildCreateUpdateBlockForPlayer(&data, primary);
    if (Unit* unit = object->ToUnit())
      units.push_back(unit);
  }
  primary->BuildCreateUpdateBlockForPlayer(&data, secondary);
  units.push_back(primary);

  WorldPacket packet;
  if (data.BuildPacket(packet))
    recording->addInitial(packet.GetOpcode(), packet.contents(), packet.size());
  for (Unit* unit : units)
    appendAuras(*recording, unit);

  Active active;
  active.recording = recording;
  active.bracket   = bracket;
  active.mapId     = bg->GetMapId();
  active.bgTypeId  = bg->GetBgTypeID();
  active.primary   = primary->GetGUID().GetCounter();
  active.secondary = secondary->GetGUID().GetCounter();
  primary->GetPosition(active.x, active.y, active.z, active.o);
  for (auto const& [guid, player] : bg->GetPlayers())
    if (player)
      active.participants.push_back(
          {player->GetGUID().GetCounter(), player->GetName(), uint8(player->GetBgTeamId()), player->getClass()});

  primary->GetSession()->SetReplayTap(recording, true);
  secondary->GetSession()->SetReplayTap(recording, false);

  std::lock_guard<std::mutex> guard(_lock);
  _active[bg->GetInstanceID()] = std::move(active);
}

void ReplayRecorder::onPlayerLeave(Battleground* bg, Player* player)
{
  if (!bg || !player)
    return;

  std::lock_guard<std::mutex> guard(_lock);
  auto                        itr = _active.find(bg->GetInstanceID());
  if (itr == _active.end())
    return;

  uint32 const guid = player->GetGUID().GetCounter();
  if (guid == itr->second.primary || guid == itr->second.secondary)
    player->GetSession()->SetReplayTap(nullptr, false);
}

void ReplayRecorder::onArenaEnd(Battleground* bg, uint8_t winnerTeam)
{
  if (!bg)
    return;

  Active active;
  {
    std::lock_guard<std::mutex> guard(_lock);
    auto                        itr = _active.find(bg->GetInstanceID());
    if (itr == _active.end())
      return;
    active = std::move(itr->second);
    _active.erase(itr);
  }

  setTap(active.primary, nullptr, false);
  setTap(active.secondary, nullptr, false);

  uint32 const duration = getMSTimeDiff(active.recording->startMs(), getMSTime());
  if (duration < MinDurationMs)
  {
    active.recording->finish();
    return;
  }

  save(std::move(active), winnerTeam, duration);
}

void ReplayRecorder::save(Active active, uint8_t winnerTeam, uint32_t durationMs)
{
  ReplayData replay;
  replay.recordStartMs = active.recording->startMs();
  replay.mapId         = active.mapId;
  replay.x             = active.x;
  replay.y             = active.y;
  replay.z             = active.z;
  replay.o             = active.o;
  replay.packets       = active.recording->finish();

  std::vector<uint8_t> blob = encode(replay);
  if (blob.empty())
  {
    LOG_ERROR("bg.arena", "Arena replay: failed to compress recording for map {}", active.mapId);
    return;
  }

  uint32 const                 id    = ++_lastId;
  CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

  CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ARENA_REPLAY);
  stmt->SetData(0, id);
  stmt->SetData(1, uint8(active.bracket));
  stmt->SetData(2, active.mapId);
  stmt->SetData(3, active.bgTypeId);
  stmt->SetData(4, winnerTeam);
  stmt->SetData(5, durationMs);
  stmt->SetData(6, std::move(blob));
  trans->Append(stmt);

  for (Participant const& p : active.participants)
  {
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ARENA_REPLAY_PLAYER);
    stmt->SetData(0, id);
    stmt->SetData(1, p.guid);
    stmt->SetData(2, p.name);
    stmt->SetData(3, p.team);
    stmt->SetData(4, p.classId);
    trans->Append(stmt);
  }

  // Retention, after the insert so the new recording counts. Deleting from
  // arena_replay cascades to arena_replay_player.
  Config const& cfg = config();
  if (cfg.keepDays)
    trans->Append("DELETE FROM arena_replay WHERE created_at < NOW() - INTERVAL {} DAY", cfg.keepDays);
  if (cfg.maxTotal)
    trans->Append("DELETE FROM arena_replay WHERE id <= (SELECT id FROM (SELECT id FROM arena_replay ORDER BY id DESC "
                  "LIMIT 1 OFFSET {}) t)",
                  cfg.maxTotal);
  if (cfg.maxPerPlayer)
    for (Participant const& p : active.participants)
      trans->Append("DELETE FROM arena_replay WHERE id IN (SELECT replay_id FROM (SELECT replay_id FROM "
                    "arena_replay_player WHERE guid = {} ORDER BY replay_id DESC LIMIT 18446744073709551615 "
                    "OFFSET {}) t)",
                    p.guid, cfg.maxPerPlayer);

  CharacterDatabase.CommitTransaction(trans);

  LOG_INFO("bg.arena", "Arena replay #{} saved: {} on map {}, {} packets, {}s", id, bracketName(active.bracket),
           active.mapId, replay.packets.size(), durationMs / IN_MILLISECONDS);
}
} // namespace arenacraft::replay
