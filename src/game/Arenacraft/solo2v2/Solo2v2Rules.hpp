#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Rated 2v2 has no shared teams: every character owns a single-member 2v2 ArenaTeam and a party of two queues
// with their two own teams (see ai-doc/solo2v2.md).
namespace arenacraft::solo2v2
{
inline constexpr uint32_t InitialRating = 1400;
inline constexpr uint32_t InitialMmr    = 1500;

// Largest MMR difference allowed between the two members of a rated 2v2 party.
inline constexpr uint32_t MaxPartyMmrGap = 300;

inline std::string teamNameFor(std::string_view characterName) { return std::string(characterName) + "'s 2v2"; }

inline constexpr bool partyMmrGapAllowed(uint32_t mmrA, uint32_t mmrB)
{
  return (mmrA > mmrB ? mmrA - mmrB : mmrB - mmrA) <= MaxPartyMmrGap;
}

// Matchmaker rating the party is queued with: the mean of both members, as the core does for a team's group.
inline constexpr uint32_t partyMmr(uint32_t mmrA, uint32_t mmrB) { return (mmrA + mmrB) / 2; }
} // namespace arenacraft::solo2v2
