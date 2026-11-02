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

void fillTeam(std::array<RatingUpdate, 6>& updates, std::size_t offset, Team const& team, int32_t mmrDelta, bool won)
{
  QueuedPlayer const* const players[3] = {&team.melee, &team.caster, &team.healer};
  for (std::size_t i = 0; i < 3; ++i)
  {
    int32_t const ratingDelta = personalRatingDelta(players[i]->rating, players[i]->mmr, mmrDelta, won);
    updates[offset + i]       = RatingUpdate{players[i]->id, applyDelta(players[i]->mmr, mmrDelta),
                                       applyDelta(players[i]->rating, ratingDelta), mmrDelta, ratingDelta};
  }
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

int32_t personalRatingDelta(uint32_t rating, uint32_t mmr, int32_t mmrDelta, bool won)
{
  int32_t const gap     = static_cast<int32_t>(mmr) - static_cast<int32_t>(rating);
  int32_t       catchup = static_cast<int32_t>(std::lround(static_cast<double>(gap) * tuning::RatingCatchupRate));

  // Any non-zero gap must eventually close; rounding alone would freeze small
  // offsets.
  if (catchup == 0 && gap != 0)
    catchup = gap > 0 ? 1 : -1;

  // Rating follows the same result-based movement as MMR, plus a pull towards
  // MMR. Clamp to the match result so a win never removes rating and a loss
  // never rewards it, even while a large deficit is being corrected.
  int32_t const delta = mmrDelta + catchup;
  if (won)
    return delta < 0 ? 0 : delta;
  return delta > 0 ? 0 : delta;
}

std::array<RatingUpdate, 6> resolveMatch(Match const& match, MatchResult result)
{
  bool const  teamAWins = result == MatchResult::TeamAWin;
  Team const& winner    = teamAWins ? match.a : match.b;
  Team const& loser     = teamAWins ? match.b : match.a;

  int32_t const mmrDelta = winningRatingDelta(teamAverageMmr(winner), teamAverageMmr(loser));

  std::array<RatingUpdate, 6> updates{};
  fillTeam(updates, 0, match.a, teamAWins ? mmrDelta : -mmrDelta, teamAWins);
  fillTeam(updates, 3, match.b, teamAWins ? -mmrDelta : mmrDelta, !teamAWins);
  return updates;
}
} // namespace arenacraft::soloq
