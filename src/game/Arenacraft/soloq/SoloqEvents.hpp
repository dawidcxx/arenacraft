#pragma once

#include "Outcome.hpp"
#include "Types.hpp"

#include <array>
#include <cstdint>
#include <string>

class Battleground;

namespace arenacraft::soloq
{
// Redis channel and `event` discriminator for a popped matchup. This is a
// public contract consumed outside the core; keep it stable and additive.
inline constexpr char const* MatchupChannel        = "soloq-matchup";
inline constexpr char const* MatchupEventName      = "soloq.matchup";
inline constexpr char const* MatchupEndedChannel   = "soloq-matchup-ended";
inline constexpr char const* MatchupEndedEventName = "soloq.matchup.ended";

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

// Post-match rating movement for one participant. Only meaningful when the
// ended event's `finished` flag is true.
struct ParticipantResult
{
  int32_t  ratingDelta;
  uint32_t ratingAfter;
  uint32_t mmrAfter;
};

// Why the matchup ended. Serialized as "finished", "cancelled" (the arena
// opened short-handed and was cancelled) or "declined" (invites were never
// accepted / the arena was destroyed without finishing).
enum class MatchupEndReason : uint8_t
{
  Finished,
  Cancelled,
  Declined
};

struct MatchupEndedEvent
{
  uint32_t bgInstanceId;
  // true when the arena ended with a winner, false when it was cancelled at
  // start or destroyed without ever finishing (see reason).
  bool             finished;
  MatchupEndReason reason;
  // TEAM_ALLIANCE (team A) or TEAM_HORDE (team B); TEAM_NEUTRAL when aborted or
  // cancelled.
  TeamId winner;
  // Pop time (same value the matchup event reported) and the time the arena
  // ended, both milliseconds since the Unix epoch.
  uint64_t startedAtMs;
  uint64_t endedAtMs;
  // Team A first, then team B; three players each, melee/caster/healer order.
  std::array<ParticipantInfo, 6> participants;
  // Filled from resolveMatch when finished; zero-initialized when aborted.
  std::array<ParticipantResult, 6> results;
};

// Pure serializer for the ended event. Unit-tested.
std::string buildMatchupEndedPayload(MatchupEndedEvent const& event);

// Serializes and publishes the ended event. Best-effort like publishMatchup.
bool publishMatchupEnded(MatchupEndedEvent const& event);

// Resolves the finished/aborted match into a MatchupEndedEvent. `updates` is
// the resolveMatch output for a finished match and is ignored when aborted.
MatchupEndedEvent makeMatchupEndedEvent(Match const& match, Battleground const* arena, bool finished,
                                        MatchupEndReason reason, uint64_t startedAtMs, TeamId winner = TEAM_NEUTRAL,
                                        std::array<RatingUpdate, 6> const* updates = nullptr);
} // namespace arenacraft::soloq
