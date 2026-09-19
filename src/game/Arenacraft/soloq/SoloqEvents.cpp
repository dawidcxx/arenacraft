#include "SoloqEvents.hpp"

#include "AccountMgr.h"
#include "Battleground.h"
#include "CharacterCache.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Outcome.hpp"
#include "Player.h"
#include "RedisConn.h"
#include "WorldSession.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

namespace arenacraft::soloq
{
namespace
{
char const* RoleName(Role role)
{
  switch (role)
  {
  case Role::Melee:
    return "melee";
  case Role::Caster:
    return "caster";
  case Role::Healer:
    return "healer";
  }
  return "unknown";
}

char const* FactionName(TeamId teamId)
{
  switch (teamId)
  {
  case TEAM_ALLIANCE:
    return "alliance";
  case TEAM_HORDE:
    return "horde";
  default:
    return "neutral";
  }
}

std::string EscapeJson(std::string_view text)
{
  std::string out;
  out.reserve(text.size());
  for (char const c : text)
  {
    switch (c)
    {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
        out += buf;
      }
      else
        out += c;
      break;
    }
  }
  return out;
}

std::string GuidToHex(PlayerId id)
{
  char buf[19];
  std::snprintf(buf, sizeof(buf), "0x%016llx", static_cast<unsigned long long>(id));
  return buf;
}

void AppendPlayer(std::string& out, ParticipantInfo const& player)
{
  out += "{\"guid\":\"";
  out += GuidToHex(player.id);
  out += "\",\"characterName\":\"";
  out += EscapeJson(player.characterName);
  out += "\",\"accountId\":";
  out += std::to_string(player.accountId);
  out += ",\"accountName\":\"";
  out += EscapeJson(player.accountName);
  out += "\",\"classId\":";
  out += std::to_string(static_cast<unsigned>(player.classId));
  out += ",\"specIndex\":";
  out += std::to_string(static_cast<unsigned>(player.specIndex));
  out += ",\"role\":\"";
  out += RoleName(player.role);
  out += "\",\"teamId\":";
  out += std::to_string(static_cast<int>(player.teamId));
  out += ",\"faction\":\"";
  out += FactionName(player.teamId);
  out += "\",\"rating\":";
  out += std::to_string(player.rating);
  out += ",\"mmr\":";
  out += std::to_string(player.mmr);
  out += "}";
}

uint32_t AverageRating(std::array<ParticipantInfo, 6> const& participants, std::size_t offset)
{
  return (participants[offset].rating + participants[offset + 1].rating + participants[offset + 2].rating) / 3;
}

uint32_t AverageMmr(std::array<ParticipantInfo, 6> const& participants, std::size_t offset)
{
  return (participants[offset].mmr + participants[offset + 1].mmr + participants[offset + 2].mmr) / 3;
}

void AppendTeam(std::string& out, std::array<ParticipantInfo, 6> const& participants, std::size_t offset)
{
  out += "{\"teamId\":";
  out += std::to_string(static_cast<int>(participants[offset].teamId));
  out += ",\"faction\":\"";
  out += FactionName(participants[offset].teamId);
  out += "\",\"averageRating\":";
  out += std::to_string(AverageRating(participants, offset));
  out += ",\"averageMmr\":";
  out += std::to_string(AverageMmr(participants, offset));
  out += ",\"players\":[";
  for (std::size_t i = 0; i < 3; ++i)
  {
    if (i)
      out += ',';
    AppendPlayer(out, participants[offset + i]);
  }
  out += "]}";
}

ParticipantInfo ResolveParticipant(QueuedPlayer const& queued, Role role)
{
  ParticipantInfo info{};
  info.id        = queued.id;
  info.classId   = queued.classId;
  info.specIndex = queued.specIndex;
  info.role      = role;
  info.teamId    = queued.teamId;
  info.rating    = queued.rating;
  info.mmr       = queued.mmr;

  ObjectGuid const guid{queued.id};
  if (Player* player = ObjectAccessor::FindPlayer(guid))
  {
    info.characterName = player->GetName();
    if (WorldSession* session = player->GetSession())
    {
      info.accountId   = session->GetAccountId();
      info.accountName = session->GetAccountName();
    }
  }
  else
  {
    sCharacterCache->GetCharacterNameByGuid(guid, info.characterName);
    info.accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
  }

  if (info.accountName.empty() && info.accountId != 0)
    AccountMgr::GetName(info.accountId, info.accountName);

  return info;
}
} // namespace

std::string buildMatchupPayload(MatchupEvent const& event)
{
  uint32_t const avgRatingA = AverageRating(event.participants, 0);
  uint32_t const avgMmrA    = AverageMmr(event.participants, 0);
  uint32_t const avgRatingB = AverageRating(event.participants, 3);
  uint32_t const avgMmrB    = AverageMmr(event.participants, 3);
  int32_t const  deltaAWin  = winningRatingDelta(avgMmrA, avgMmrB);
  int32_t const  deltaBWin  = winningRatingDelta(avgMmrB, avgMmrA);

  std::string out;
  out.reserve(1024);
  out += "{\"event\":\"";
  out += MatchupEventName;
  out += "\",\"instanceId\":";
  out += std::to_string(event.bgInstanceId);
  out += ",\"arenaType\":";
  out += std::to_string(static_cast<unsigned>(event.arenaType));

  out += ",\"matchup\":{\"instanceId\":";
  out += std::to_string(event.bgInstanceId);
  out += ",\"arenaType\":";
  out += std::to_string(static_cast<unsigned>(event.arenaType));
  out += ",\"mapId\":";
  out += std::to_string(event.mapId);
  out += ",\"bracketId\":";
  out += std::to_string(static_cast<unsigned>(event.bracketId));
  out += ",\"queueType\":";
  out += std::to_string(static_cast<unsigned>(event.queueType));
  out += ",\"startedAtMs\":";
  out += std::to_string(event.startedAtMs);
  out += "}";

  out += ",\"players\":[";
  for (std::size_t i = 0; i < event.participants.size(); ++i)
  {
    if (i)
      out += ',';
    AppendPlayer(out, event.participants[i]);
  }
  out += "]";

  out += ",\"teamA\":";
  AppendTeam(out, event.participants, 0);
  out += ",\"teamB\":";
  AppendTeam(out, event.participants, 3);

  out += ",\"rating\":{\"teamA\":{\"averageRating\":";
  out += std::to_string(avgRatingA);
  out += ",\"averageMmr\":";
  out += std::to_string(avgMmrA);
  out += ",\"winDelta\":";
  out += std::to_string(deltaAWin);
  out += ",\"lossDelta\":";
  out += std::to_string(-deltaBWin);
  out += "},\"teamB\":{\"averageRating\":";
  out += std::to_string(avgRatingB);
  out += ",\"averageMmr\":";
  out += std::to_string(avgMmrB);
  out += ",\"winDelta\":";
  out += std::to_string(deltaBWin);
  out += ",\"lossDelta\":";
  out += std::to_string(-deltaAWin);
  out += "}}";

  out += "}";
  return out;
}

bool publishMatchup(MatchupEvent const& event)
{
  return sRedisConn.publish(MatchupChannel, buildMatchupPayload(event));
}

MatchupEvent makeMatchupEvent(Match const& match, Battleground const* arena)
{
  MatchupEvent event{};
  event.bgInstanceId = arena ? arena->GetInstanceID() : 0;
  event.arenaType    = arena ? arena->GetArenaType() : 0;
  event.mapId        = arena ? arena->GetMapId() : 0;
  event.bracketId    = arena ? static_cast<uint8_t>(arena->GetBracketId()) : 0;
  event.queueType    = static_cast<uint8_t>(BATTLEGROUND_QUEUE_5v5);
  event.startedAtMs  = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(GameTime::GetSystemTime().time_since_epoch()).count());

  std::array<QueuedPlayer const*, 6> const players = playersOf(match);
  std::array<Role, 3> const                roles   = {Role::Melee, Role::Caster, Role::Healer};
  for (std::size_t i = 0; i < players.size(); ++i)
    event.participants[i] = ResolveParticipant(*players[i], roles[i % 3]);

  return event;
}

std::string buildMatchupEndedPayload(MatchupEndedEvent const& event)
{
  std::string out;
  out.reserve(1024);
  out += "{\"event\":\"";
  out += MatchupEndedEventName;
  out += "\",\"instanceId\":";
  out += std::to_string(event.bgInstanceId);
  out += ",\"finished\":";
  out += event.finished ? "true" : "false";
  out += ",\"players\":[";
  for (std::size_t i = 0; i < event.participants.size(); ++i)
  {
    if (i)
      out += ',';
    AppendPlayer(out, event.participants[i]);
  }
  out += "]}";
  return out;
}

bool publishMatchupEnded(MatchupEndedEvent const& event)
{
  return sRedisConn.publish(MatchupEndedChannel, buildMatchupEndedPayload(event));
}

MatchupEndedEvent makeMatchupEndedEvent(Match const& match, Battleground const* arena, bool finished)
{
  MatchupEndedEvent event{};
  event.bgInstanceId = arena ? arena->GetInstanceID() : 0;
  event.finished     = finished;

  std::array<QueuedPlayer const*, 6> const players = playersOf(match);
  std::array<Role, 3> const                roles   = {Role::Melee, Role::Caster, Role::Healer};
  for (std::size_t i = 0; i < players.size(); ++i)
    event.participants[i] = ResolveParticipant(*players[i], roles[i % 3]);

  return event;
}
} // namespace arenacraft::soloq
