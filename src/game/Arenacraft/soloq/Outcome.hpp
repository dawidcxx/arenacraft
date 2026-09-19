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
  int32_t  delta;
};

std::array<RatingUpdate, 6> resolveMatch(Match const& match, MatchResult result);
} // namespace arenacraft::soloq
