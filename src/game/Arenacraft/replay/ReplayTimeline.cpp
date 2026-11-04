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

namespace
{
constexpr std::string_view SpectatorPrefix = "ASSUN\t";
// ArenaSpectator::CreatePacket: u8 type, u32 lang, u64 sender, u32, u64 receiver, u32 length (incl. the 0), text
constexpr std::size_t ChatHeaderSize = 1 + 4 + 8 + 4 + 8 + 4;

std::optional<int64_t> parseInt(std::string_view text)
{
  int64_t value     = 0;
  auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || text.empty())
    return std::nullopt;
  return value;
}

// Splits "a,b,c" into its fields.
std::vector<std::string_view> splitFields(std::string_view text)
{
  std::vector<std::string_view> fields;
  for (std::size_t start = 0;;)
  {
    std::size_t comma = text.find(',', start);
    fields.push_back(text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
    if (comma == std::string_view::npos)
      return fields;
    start = comma + 1;
  }
}

std::string joinFields(std::vector<std::string_view> const& fields)
{
  std::string out;
  for (std::size_t i = 0; i < fields.size(); ++i)
  {
    if (i)
      out += ',';
    out += fields[i];
  }
  return out;
}
} // namespace

std::optional<std::string_view> spectatorMessageOf(RecordedPacket const& packet)
{
  if (packet.opcode != SMSG_MESSAGECHAT || packet.data.size() <= ChatHeaderSize)
    return std::nullopt;
  uint32_t length = 0;
  for (int b = 3; b >= 0; --b)
    length = (length << 8) | packet.data[ChatHeaderSize - 4 + std::size_t(b)];
  if (length == 0 || ChatHeaderSize + length > packet.data.size())
    return std::nullopt;
  std::string_view text(reinterpret_cast<char const*>(packet.data.data()) + ChatHeaderSize, length - 1);
  if (!text.starts_with(SpectatorPrefix))
    return std::nullopt;
  return text;
}

std::optional<std::string> adjustSpectatorMessage(std::string_view message, uint32_t skippedMs)
{
  // "ASSUN\t0x<guid>;KEY=value;"
  std::size_t const keyStart = message.find(';');
  std::size_t const eq       = message.find('=', keyStart);
  std::size_t const end      = message.find(';', eq);
  if (keyStart == std::string_view::npos || eq == std::string_view::npos || end == std::string_view::npos)
    return std::string(message);

  std::string_view const key   = message.substr(keyStart + 1, eq - keyStart - 1);
  std::string_view const value = message.substr(eq + 1, end - eq - 1);
  if (key == "SPE")
    return std::nullopt; // a cast that started back then, long over

  std::vector<std::string_view> fields = splitFields(value);
  std::string                   number;
  if (key == "ACD" && fields.size() == 3) // spell, seconds left, full cooldown
  {
    std::optional<int64_t> left = parseInt(fields[1]);
    if (!left)
      return std::string(message);
    int64_t const now = *left - int64_t(skippedMs / 1000);
    if (now <= 0)
      return std::nullopt;
    number    = std::to_string(now);
    fields[1] = number;
  }
  else if (key == "TIM" && fields.size() == 1) // seconds until the arena times out
  {
    std::optional<int64_t> left = parseInt(fields[0]);
    if (!left)
      return std::string(message);
    number    = std::to_string(std::max<int64_t>(*left - int64_t(skippedMs / 1000), 0));
    fields[0] = number;
  }
  else if (key == "AUR" && fields.size() == 8) // removed, stacks, ms left, full duration, spell, dispel, debuff, caster
  {
    std::optional<int64_t> left = parseInt(fields[2]);
    if (fields[0] != "0" || !left || *left <= 0) // removals and auras without a duration stay as they are
      return std::string(message);
    int64_t const now = *left - int64_t(skippedMs);
    if (now <= 0)
      return std::nullopt;
    number    = std::to_string(now);
    fields[2] = number;
  }
  else
    return std::string(message);

  std::string out(message.substr(0, eq + 1));
  out += joinFields(fields);
  out += message.substr(end);
  return out;
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
