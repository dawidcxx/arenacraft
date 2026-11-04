#include "ReplayTimeline.hpp"

#include "Opcodes.h"
#include "ReplayRecorder.hpp"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <unordered_set>

namespace arenacraft::replay
{
namespace
{
std::optional<uint32_t> parseUnsigned(std::string_view text)
{
  uint32_t value    = 0;
  auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || text.empty())
    return std::nullopt;
  return value;
}

// Seconds of "95", "95s" or "1:35".
std::optional<uint32_t> parseSeconds(std::string_view text)
{
  if (!text.empty() && (text.back() == 's' || text.back() == 'S'))
    text.remove_suffix(1);

  if (std::size_t colon = text.find(':'); colon != std::string_view::npos)
  {
    std::optional<uint32_t> minutes = parseUnsigned(text.substr(0, colon));
    std::optional<uint32_t> seconds = parseUnsigned(text.substr(colon + 1));
    if (!minutes || !seconds || *seconds >= 60)
      return std::nullopt;
    return *minutes * 60 + *seconds;
  }
  return parseUnsigned(text);
}

bool isSplineOpcode(uint16_t opcode) { return opcode == SMSG_MONSTER_MOVE; }

// Opcodes whose payload starts with the packed guid of the object they are about.
bool startsWithPackedGuid(uint16_t opcode)
{
  return isMovementOpcode(opcode) || isSplineOpcode(opcode) || opcode == SMSG_AURA_UPDATE ||
         opcode == SMSG_AURA_UPDATE_ALL || opcode == SMSG_SPELL_START || opcode == SMSG_SPELL_GO;
}
} // namespace

std::optional<uint32_t> parseSeekTarget(std::string_view text, uint32_t currentMs, uint32_t durationMs)
{
  while (!text.empty() && text.front() == ' ')
    text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ')
    text.remove_suffix(1);
  if (text.empty())
    return std::nullopt;

  int64_t target = 0;
  if (text.front() == '+' || text.front() == '-')
  {
    std::optional<uint32_t> seconds = parseSeconds(text.substr(1));
    if (!seconds)
      return std::nullopt;
    int64_t const delta = int64_t(*seconds) * 1000;
    target              = int64_t(currentMs) + (text.front() == '+' ? delta : -delta);
  }
  else
  {
    std::optional<uint32_t> seconds = parseSeconds(text);
    if (!seconds)
      return std::nullopt;
    target = int64_t(*seconds) * 1000;
  }

  return uint32_t(std::clamp<int64_t>(target, 0, durationMs));
}

bool isTransientOpcode(uint16_t opcode)
{
  switch (opcode)
  {
  case SMSG_SPELL_START:
  case SMSG_SPELL_GO:
  case SMSG_SPELL_FAILURE:
  case SMSG_SPELL_FAILED_OTHER:
  case SMSG_SPELLLOGEXECUTE:
  case SMSG_SPELLNONMELEEDAMAGELOG:
  case SMSG_ATTACKERSTATEUPDATE:
  case SMSG_SPELLHEALLOG:
  case SMSG_PERIODICAURALOG:
  case SMSG_SPELLENERGIZELOG:
  case SMSG_SPELLLOGMISS:
  case SMSG_SPELLDISPELLOG:
  case SMSG_SPELLSTEALLOG:
  case SMSG_SPELLBREAKLOG:
  case SMSG_SPELLINSTAKILLLOG:
  case SMSG_SPELLDAMAGESHIELD:
  case SMSG_SPELLORDAMAGE_IMMUNE:
  case SMSG_PROCRESIST:
  case SMSG_RESISTLOG:
  case SMSG_PLAY_SPELL_VISUAL:
  case SMSG_PLAY_SPELL_IMPACT:
  case SMSG_PLAY_SOUND:
  case SMSG_PLAY_OBJECT_SOUND:
  case SMSG_EMOTE:
  case SMSG_AI_REACTION:
    return true;
  default:
    return false;
  }
}

std::vector<std::size_t> planJump(std::vector<RecordedPacket> const& packets, std::size_t from, uint32_t targetMs,
                                  std::size_t& next)
{
  std::vector<std::size_t>                  plan;
  std::unordered_map<uint64_t, std::size_t> lastMove; // object -> index of its last movement packet

  std::size_t i = from;
  for (; i < packets.size() && packets[i].offsetMs <= targetMs; ++i)
  {
    RecordedPacket const& packet = packets[i];
    if (isTransientOpcode(packet.opcode))
      continue;

    if (isMovementOpcode(packet.opcode) || isSplineOpcode(packet.opcode))
    {
      std::size_t pos = 0;
      if (std::optional<uint64_t> guid = readPackedGuid(packet.data, pos))
      {
        lastMove[*guid] = i;
        continue;
      }
    }

    plan.push_back(i);
  }
  next = i;

  std::vector<std::size_t> moves;
  moves.reserve(lastMove.size());
  for (auto const& [guid, index] : lastMove)
    moves.push_back(index);
  std::sort(moves.begin(), moves.end());
  plan.insert(plan.end(), moves.begin(), moves.end());
  return plan;
}

std::vector<uint64_t> collectObjectGuids(std::vector<RecordedPacket> const& packets)
{
  std::unordered_set<uint64_t> guids;
  for (RecordedPacket const& packet : packets)
  {
    if (packet.opcode == SMSG_DESTROY_OBJECT && packet.data.size() >= 8)
    {
      uint64_t guid = 0;
      for (int b = 7; b >= 0; --b)
        guid = (guid << 8) | packet.data[std::size_t(b)];
      guids.insert(guid);
      continue;
    }

    if (!startsWithPackedGuid(packet.opcode))
      continue;

    std::size_t pos = 0;
    if (std::optional<uint64_t> guid = readPackedGuid(packet.data, pos); guid && *guid)
      guids.insert(*guid);
  }
  return {guids.begin(), guids.end()};
}
} // namespace arenacraft::replay
