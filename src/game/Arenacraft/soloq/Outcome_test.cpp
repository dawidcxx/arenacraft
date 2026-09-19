#include <doctest/doctest.h>

#include "Outcome.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using arenacraft::soloq::Match;
using arenacraft::soloq::MatchResult;
using arenacraft::soloq::PlayerId;
using arenacraft::soloq::QueuedPlayer;
using arenacraft::soloq::RatingUpdate;
using arenacraft::soloq::resolveMatch;
using arenacraft::soloq::tuning::InitialRating;
using SoloqTeam = arenacraft::soloq::Team;

namespace
{
QueuedPlayer player(PlayerId id, uint32_t mmr)
{
  return QueuedPlayer{id, CLASS_WARRIOR, 0, InitialRating, mmr, TEAM_ALLIANCE};
}

Match makeMatch(std::array<uint32_t, 6> const& mmrs)
{
  return Match{
      SoloqTeam{player(1, mmrs[0]), player(2, mmrs[1]), player(3, mmrs[2])},
      SoloqTeam{player(4, mmrs[3]), player(5, mmrs[4]), player(6, mmrs[5])},
  };
}
} // namespace

TEST_CASE("resolveMatch rewards the winner and deducts the loser, zero-sum")
{
  Match const                       match   = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamAWin);

  int64_t sum = 0;
  for (std::size_t i = 0; i < 3; ++i)
  {
    CHECK(updates[i].delta > 0);
    CHECK(static_cast<int64_t>(updates[i].mmr) == 1500 + static_cast<int64_t>(updates[i].delta));
    sum += updates[i].delta;
  }
  for (std::size_t i = 3; i < 6; ++i)
  {
    CHECK(updates[i].delta < 0);
    CHECK(static_cast<int64_t>(updates[i].mmr) == 1500 + static_cast<int64_t>(updates[i].delta));
    sum += updates[i].delta;
  }
  CHECK(sum == 0);
}

TEST_CASE("resolveMatch rewards the underdog more than the favourite")
{
  Match const even     = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  Match const underdog = makeMatch({1000, 1000, 1000, 1500, 1500, 1500});

  CHECK(resolveMatch(underdog, MatchResult::TeamAWin)[0].delta > resolveMatch(even, MatchResult::TeamAWin)[0].delta);
}

TEST_CASE("resolveMatch assigns every player their own id")
{
  Match const                       match   = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamBWin);

  std::array<PlayerId, 6> const expected = {1, 2, 3, 4, 5, 6};
  for (std::size_t i = 0; i < updates.size(); ++i)
    CHECK(updates[i].id == expected[i]);
}

TEST_CASE("resolveMatch never pushes rating below zero")
{
  Match const                       match   = makeMatch({0, 0, 0, 0, 0, 0});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamAWin);

  for (std::size_t i = 3; i < 6; ++i)
  {
    CHECK(updates[i].delta < 0);
    CHECK(updates[i].mmr == 0);
  }
}
