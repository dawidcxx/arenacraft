#include "Outcome.hpp"

#include <cmath>

namespace arenacraft::soloq
{
namespace
{
uint32_t applyDelta(uint32_t value, int32_t delta)
{
  int64_t const adjusted = static_cast<int64_t>(value) + delta;
  return adjusted < 0 ? 0u : static_cast<uint32_t>(adjusted);
}

void fillTeam(std::array<RatingUpdate, 6>& updates, std::size_t offset, Team const& team, int32_t delta)
{
  QueuedPlayer const* const players[3] = {&team.melee, &team.caster, &team.healer};
  for (std::size_t i = 0; i < 3; ++i)
    updates[offset + i] =
        RatingUpdate{players[i]->id, applyDelta(players[i]->mmr, delta), applyDelta(players[i]->rating, delta), delta};
}
} // namespace

uint32_t teamAverageMmr(Team const& team) { return (team.melee.mmr + team.caster.mmr + team.healer.mmr) / 3; }

uint32_t teamAverageRating(Team const& team)
{
  return (team.melee.rating + team.caster.rating + team.healer.rating) / 3;
}

int32_t winningRatingDelta(uint32_t winnerAverage, uint32_t loserAverage)
{
  double const expected =
      1.0 / (1.0 + std::pow(10.0, (static_cast<double>(loserAverage) - static_cast<double>(winnerAverage)) /
                                      static_cast<double>(tuning::EloScale)));
  return static_cast<int32_t>(std::lround(static_cast<double>(tuning::KFactor) * (1.0 - expected)));
}

std::array<RatingUpdate, 6> resolveMatch(Match const& match, MatchResult result)
{
  bool const  teamAWins = result == MatchResult::TeamAWin;
  Team const& winner    = teamAWins ? match.a : match.b;
  Team const& loser     = teamAWins ? match.b : match.a;

  int32_t const delta = winningRatingDelta(teamAverageMmr(winner), teamAverageMmr(loser));

  std::array<RatingUpdate, 6> updates{};
  fillTeam(updates, 0, match.a, teamAWins ? delta : -delta);
  fillTeam(updates, 3, match.b, teamAWins ? -delta : delta);
  return updates;
}
} // namespace arenacraft::soloq
