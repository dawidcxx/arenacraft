#pragma once

#include "Types.hpp"

#include <array>
#include <cstdint>
#include <string>

class Battleground;

namespace arenacraft::soloq
{
// Redis channel and `event` discriminator for a popped matchup. This is a
// public contract consumed outside the core; keep it stable and additive.
inline constexpr char const* MatchupChannel   = "soloq-matchup";
inline constexpr char const* MatchupEventName = "soloq.matchup";

// Fully-resolved participant snapshot for a matchup event. Character/account
// data is filled from the live player at pop time, so consumers never have to
// hit the world or character databases themselves.
struct ParticipantInfo
{
  PlayerId    id;
  std::string characterName;
  uint32_t    accountId;
  std::string accountName;
  Classes     classId;
  uint8_t     specIndex;
  Role        role;
  TeamId      teamId;
  uint32_t    rating;
  uint32_t    mmr;
};

struct MatchupEvent
{
  uint32_t bgInstanceId;
  uint8_t  arenaType;
  uint32_t mapId;
  uint8_t  bracketId;
  uint8_t  queueType;
  uint64_t startedAtMs;
  // Team A (alliance side) first, then team B; three players each, in
  // melee/caster/healer order.
  std::array<ParticipantInfo, 6> participants;
};

// Pure serializer: no Redis, no game world. Unit-tested.
std::string buildMatchupPayload(MatchupEvent const& event);

// Serializes and publishes the event. Best-effort: returns false when Redis is
// disabled or unreachable, never throws.
bool publishMatchup(MatchupEvent const& event);

// Resolves a formed match and its arena into a MatchupEvent, pulling
// character/account names from the live players (falling back to the character
// cache). Missing players yield empty names instead of failing.
MatchupEvent makeMatchupEvent(Match const& match, Battleground const* arena);
} // namespace arenacraft::soloq
