#pragma once

#include "Types.hpp"

#include <array>
#include <cstdint>

namespace arenacraft::soloq
{
enum class MatchResult
{
  TeamAWin,
  TeamBWin
};

struct RatingUpdate
{
  PlayerId id;
  uint32_t mmr;
  uint32_t rating;
  int32_t  mmrDelta;
  int32_t  ratingDelta;
};

std::array<RatingUpdate, 6> resolveMatch(Match const& match, MatchResult result);

// Mean MMR of a team's three members.
uint32_t teamAverageMmr(Team const& team);

// Mean rating of a team's three members.
uint32_t teamAverageRating(Team const& team);

// Elo rating change for `winnerAverage` beating `loserAverage` (always >= 0).
int32_t winningRatingDelta(uint32_t winnerAverage, uint32_t loserAverage);

// Personal rating change for a player with `rating`/`mmr` whose MMR moved by
// `mmrDelta` in a won/lost match. Includes the catch-up pull towards MMR.
int32_t personalRatingDelta(uint32_t rating, uint32_t mmr, int32_t mmrDelta, bool won);
} // namespace arenacraft::soloq
