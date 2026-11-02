#include <doctest/doctest.h>

#include "Outcome.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using arenacraft::soloq::Match;
using arenacraft::soloq::MatchResult;
using arenacraft::soloq::personalRatingDelta;
using arenacraft::soloq::PlayerId;
using arenacraft::soloq::playersOf;
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

void applyTeam(SoloqTeam& team, std::array<RatingUpdate, 6> const& updates, std::size_t offset)
{
  QueuedPlayer* members[3] = {&team.melee, &team.caster, &team.healer};
  for (std::size_t i = 0; i < 3; ++i)
  {
    members[i]->mmr    = updates[offset + i].mmr;
    members[i]->rating = updates[offset + i].rating;
  }
}

int64_t gap(QueuedPlayer const* member)
{
  return static_cast<int64_t>(member->rating) - static_cast<int64_t>(member->mmr);
}
} // namespace

TEST_CASE("resolveMatch keeps MMR zero-sum and moves rating with the result")
{
  Match const                       match   = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamAWin);

  int64_t mmrSum = 0;
  for (std::size_t i = 0; i < 3; ++i)
  {
    CHECK(updates[i].mmrDelta == 16);
    CHECK(updates[i].ratingDelta == 21);
    CHECK(updates[i].mmr == 1516);
    CHECK(updates[i].rating == 1421);
    mmrSum += updates[i].mmrDelta;
  }
  for (std::size_t i = 3; i < 6; ++i)
  {
    CHECK(updates[i].mmrDelta == -16);
    CHECK(updates[i].ratingDelta == -11);
    CHECK(updates[i].mmr == 1484);
    CHECK(updates[i].rating == 1389);
    mmrSum += updates[i].mmrDelta;
  }
  CHECK(mmrSum == 0);
}

TEST_CASE("resolveMatch rewards the underdog more than the favourite")
{
  Match const even     = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  Match const underdog = makeMatch({1000, 1000, 1000, 1500, 1500, 1500});

  CHECK(resolveMatch(underdog, MatchResult::TeamAWin)[0].mmrDelta >
        resolveMatch(even, MatchResult::TeamAWin)[0].mmrDelta);
}

TEST_CASE("resolveMatch assigns every player their own id")
{
  Match const                       match   = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamBWin);

  std::array<PlayerId, 6> const expected = {1, 2, 3, 4, 5, 6};
  for (std::size_t i = 0; i < updates.size(); ++i)
    CHECK(updates[i].id == expected[i]);
}

TEST_CASE("resolveMatch never pushes MMR below zero")
{
  Match const                       match   = makeMatch({0, 0, 0, 0, 0, 0});
  std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamAWin);

  for (std::size_t i = 3; i < 6; ++i)
  {
    CHECK(updates[i].mmrDelta < 0);
    CHECK(updates[i].mmr == 0);
  }
}

TEST_CASE("personalRatingDelta pulls rating towards MMR")
{
  // In step with MMR: no catch-up.
  CHECK(personalRatingDelta(1500, 1500, 16, true) == 16);
  CHECK(personalRatingDelta(1500, 1500, -16, false) == -16);

  // 100 behind MMR: wins pay extra, losses cost less.
  CHECK(personalRatingDelta(1400, 1500, 16, true) == 21);
  CHECK(personalRatingDelta(1400, 1500, -16, false) == -11);

  // 100 ahead of MMR: wins pay less, losses cost more.
  CHECK(personalRatingDelta(1600, 1500, 16, true) == 11);
  CHECK(personalRatingDelta(1600, 1500, -16, false) == -21);

  // A large deficit corrects a loss at worst to zero.
  CHECK(personalRatingDelta(1500, 2000, -16, false) == 0);
}

TEST_CASE("a 100-point rating deficit closes over repeated matches")
{
  Match match = makeMatch({1500, 1500, 1500, 1500, 1500, 1500});

  for (int game = 0; game < 120; ++game)
  {
    std::array<RatingUpdate, 6> const updates = resolveMatch(match, MatchResult::TeamAWin);
    applyTeam(match.a, updates, 0);
    applyTeam(match.b, updates, 3);
  }

  for (QueuedPlayer const* member : playersOf(match))
  {
    CHECK(gap(member) >= -1);
    CHECK(gap(member) <= 1);
  }
}
