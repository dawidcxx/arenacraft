#include <doctest/doctest.h>

#include "Roles.hpp"
#include "SoloqQueue.hpp"

#include <chrono>
#include <set>
#include <vector>

using arenacraft::soloq::Match;
using arenacraft::soloq::PlayerId;
using arenacraft::soloq::QueuedPlayer;
using arenacraft::soloq::Role;
using arenacraft::soloq::roleFor;
using arenacraft::soloq::SoloqQueue;
using arenacraft::soloq::tuning::InitialMmr;
using SoloqTeam = arenacraft::soloq::Team;
using arenacraft::soloq::tuning::InitialRating;
using namespace std::chrono_literals;

namespace
{
QueuedPlayer player(PlayerId id, Classes classId, uint8_t specIndex, uint32_t mmr = InitialMmr,
                    TeamId teamId = TEAM_ALLIANCE)
{
  return QueuedPlayer{id, classId, specIndex, InitialRating, mmr, teamId};
}

QueuedPlayer melee(PlayerId id, uint32_t mmr = InitialMmr, TeamId teamId = TEAM_ALLIANCE)
{
  return player(id, CLASS_WARRIOR, 0, mmr, teamId);
}
QueuedPlayer caster(PlayerId id, uint32_t mmr = InitialMmr, TeamId teamId = TEAM_ALLIANCE)
{
  return player(id, CLASS_MAGE, 0, mmr, teamId);
}
QueuedPlayer healer(PlayerId id, uint32_t mmr = InitialMmr, TeamId teamId = TEAM_ALLIANCE)
{
  return player(id, CLASS_PRIEST, 1, mmr, teamId);
}

void addSet(SoloqQueue& queue, PlayerId base, uint32_t meleeMmr, uint32_t casterMmr, uint32_t healerMmr)
{
  queue.playerAddToQueue(melee(base + 0, meleeMmr));
  queue.playerAddToQueue(melee(base + 1, meleeMmr));
  queue.playerAddToQueue(caster(base + 2, casterMmr));
  queue.playerAddToQueue(caster(base + 3, casterMmr));
  queue.playerAddToQueue(healer(base + 4, healerMmr));
  queue.playerAddToQueue(healer(base + 5, healerMmr));
}

std::multiset<PlayerId> idsOf(Match const& match)
{
  std::multiset<PlayerId> ids;
  for (SoloqTeam const* team : {&match.a, &match.b})
  {
    ids.insert(team->melee.id);
    ids.insert(team->caster.id);
    ids.insert(team->healer.id);
  }
  return ids;
}
} // namespace

TEST_CASE("SoloqQueue rejects duplicate players")
{
  SoloqQueue queue;

  CHECK(queue.playerAddToQueue(melee(1)));
  CHECK_FALSE(queue.playerAddToQueue(melee(1)));
  CHECK(queue.size() == 1);
}

TEST_CASE("SoloqQueue rejects class and spec combinations that have no role")
{
  SoloqQueue queue;

  CHECK_FALSE(queue.playerAddToQueue(player(1, CLASS_NONE, 0)));
  CHECK_FALSE(queue.playerAddToQueue(player(2, CLASS_MAGE, 3)));
  CHECK_FALSE(queue.playerAddToQueue(player(3, CLASS_WARRIOR, 99)));
  CHECK(queue.empty());
}

TEST_CASE("SoloqQueue removes queued players and reports unknown ids")
{
  SoloqQueue queue;
  REQUIRE(queue.playerAddToQueue(melee(1)));

  CHECK(queue.playerRemoveFromQueue(1));
  CHECK_FALSE(queue.playerRemoveFromQueue(1));
  CHECK_FALSE(queue.playerRemoveFromQueue(42));
  CHECK(queue.empty());
}

TEST_CASE("SoloqQueue needs two players of every role before matching")
{
  SoloqQueue queue;
  queue.playerAddToQueue(melee(1));
  queue.playerAddToQueue(melee(2));
  queue.playerAddToQueue(caster(3));
  queue.playerAddToQueue(caster(4));
  queue.playerAddToQueue(healer(5));

  CHECK(queue.update(0ms).empty());
  CHECK(queue.size() == 5);

  queue.playerAddToQueue(healer(6));

  CHECK(queue.update(0ms).size() == 1);
  CHECK(queue.empty());
}

TEST_CASE("a match is two teams of one melee, one caster and one healer")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1500);

  std::vector<Match> const matches = queue.update(0ms);
  REQUIRE(matches.size() == 1);

  for (SoloqTeam const* team : {&matches[0].a, &matches[0].b})
  {
    CHECK(roleFor(team->melee.classId, team->melee.specIndex) == Role::Melee);
    CHECK(roleFor(team->caster.classId, team->caster.specIndex) == Role::Caster);
    CHECK(roleFor(team->healer.classId, team->healer.specIndex) == Role::Healer);
  }

  std::multiset<PlayerId> const ids = idsOf(matches[0]);
  CHECK(ids.size() == 6);
  CHECK(ids.contains(100));
  CHECK(ids.contains(105));
}

TEST_CASE("update drains matched players and leaves the rest queued")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1500);
  queue.playerAddToQueue(melee(900));

  std::vector<Match> const matches = queue.update(0ms);
  REQUIRE(matches.size() == 1);
  CHECK(queue.size() == 1);
  CHECK(queue.contains(900));
  CHECK_FALSE(queue.contains(100));
  CHECK(queue.update(0ms).empty());
}

TEST_CASE("update forms multiple matches in a single call")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1500);
  addSet(queue, 200, 1500, 1500, 1500);

  CHECK(queue.update(0ms).size() == 2);
  CHECK(queue.empty());
}

TEST_CASE("matching tolerance starts at the initial window")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1650); // 150 gap == InitialWindow

  CHECK(queue.update(0ms).size() == 1);
  CHECK(queue.empty());
}

TEST_CASE("matching tolerance grows by 50 every 30 seconds up to the cap")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1700); // 200 gap: 150 base + one step

  CHECK(queue.update(29s).empty());
  CHECK(queue.size() == 6);
  CHECK(queue.update(1s).size() == 1);
  CHECK(queue.empty());
}

TEST_CASE("matching tolerance never exceeds the cap")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 2200); // 700 gap > MaxWindow

  CHECK(queue.update(1h).empty());
  CHECK(queue.size() == 6);
}

TEST_CASE("update prefers the tightest available match")
{
  SoloqQueue queue;
  addSet(queue, 100, 1500, 1500, 1500);
  addSet(queue, 200, 1800, 1900, 1850);

  std::vector<Match> const matches = queue.update(300s);
  REQUIRE(matches.size() == 2);

  std::vector<uint32_t> const firstMmrs = {
      matches[0].a.melee.mmr, matches[0].a.caster.mmr, matches[0].a.healer.mmr,
      matches[0].b.melee.mmr, matches[0].b.caster.mmr, matches[0].b.healer.mmr,
  };
  for (uint32_t const mmr : firstMmrs)
    CHECK(mmr == 1500);
}

TEST_CASE("teams are split to minimise the MMR difference")
{
  SoloqQueue queue;
  queue.playerAddToQueue(melee(1, 1500));
  queue.playerAddToQueue(melee(2, 1500));
  queue.playerAddToQueue(caster(3, 1400));
  queue.playerAddToQueue(caster(4, 1600));
  queue.playerAddToQueue(healer(5, 1400));
  queue.playerAddToQueue(healer(6, 1600));

  std::vector<Match> const matches = queue.update(300s);
  REQUIRE(matches.size() == 1);

  auto const sum = [](SoloqTeam const& team) -> uint32_t { return team.melee.mmr + team.caster.mmr + team.healer.mmr; };

  CHECK(sum(matches[0].a) == 4500);
  CHECK(sum(matches[0].b) == 4500);
}

TEST_CASE("SoloqQueue does not stack a class within a team")
{
  SoloqQueue queue;
  queue.playerAddToQueue(melee(1));
  queue.playerAddToQueue(melee(2));
  queue.playerAddToQueue(player(3, CLASS_PRIEST, 2)); // Shadow (Caster)
  queue.playerAddToQueue(player(4, CLASS_PRIEST, 2)); // Shadow (Caster)
  queue.playerAddToQueue(player(5, CLASS_PRIEST, 1)); // Holy (Healer)
  queue.playerAddToQueue(player(6, CLASS_PRIEST, 1)); // Holy (Healer)

  // Team A gets one warrior + one priest caster + one priest healer, so priests
  // stack in every possible partition; no match is valid.
  CHECK(queue.update(0ms).empty());
  CHECK(queue.size() == 6);
}

TEST_CASE("SoloqQueue allows cross-faction opponents but not mixed-faction teams")
{
  SoloqQueue split;
  split.playerAddToQueue(melee(1));
  split.playerAddToQueue(caster(2));
  split.playerAddToQueue(healer(3));
  split.playerAddToQueue(melee(4, InitialMmr, TEAM_HORDE));
  split.playerAddToQueue(caster(5, InitialMmr, TEAM_HORDE));
  split.playerAddToQueue(healer(6, InitialMmr, TEAM_HORDE));

  std::vector<Match> const matches = split.update(0ms);
  REQUIRE(matches.size() == 1);

  for (SoloqTeam const* team : {&matches[0].a, &matches[0].b})
  {
    CHECK(team->melee.teamId == team->caster.teamId);
    CHECK(team->melee.teamId == team->healer.teamId);
  }

  SoloqQueue sixHorde;
  sixHorde.playerAddToQueue(melee(1, InitialMmr, TEAM_HORDE));
  sixHorde.playerAddToQueue(melee(2, InitialMmr, TEAM_HORDE));
  sixHorde.playerAddToQueue(caster(3, InitialMmr, TEAM_HORDE));
  sixHorde.playerAddToQueue(caster(4, InitialMmr, TEAM_HORDE));
  sixHorde.playerAddToQueue(healer(5, InitialMmr, TEAM_HORDE));
  sixHorde.playerAddToQueue(healer(6, InitialMmr, TEAM_HORDE));

  CHECK(sixHorde.update(0ms).size() == 1);

  SoloqQueue fourTwo;
  fourTwo.playerAddToQueue(melee(1));
  fourTwo.playerAddToQueue(melee(2));
  fourTwo.playerAddToQueue(caster(3));
  fourTwo.playerAddToQueue(healer(4));
  fourTwo.playerAddToQueue(caster(5, InitialMmr, TEAM_HORDE));
  fourTwo.playerAddToQueue(healer(6, InitialMmr, TEAM_HORDE));

  CHECK(fourTwo.update(0ms).empty());
  CHECK(fourTwo.size() == 6);
}
