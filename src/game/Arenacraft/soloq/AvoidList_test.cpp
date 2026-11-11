#include <doctest/doctest.h>

#include "AvoidList.hpp"

#include <algorithm>

using namespace arenacraft::soloq;

namespace
{
constexpr int64_t Hour = 60 * 60;
constexpr int64_t T0   = 1'800'000'000;
} // namespace

TEST_CASE("avoid list: two slots, then none until a cooldown ends")
{
  AvoidList list;
  CHECK(list.add(1, 2, 1002, T0) == AvoidList::AddResult::Added);
  CHECK(list.add(1, 3, 1003, T0) == AvoidList::AddResult::Added);
  CHECK(list.add(1, 4, 1004, T0) == AvoidList::AddResult::NoFreeSlot);
  CHECK(list.avoids(1, 2));
  CHECK(list.avoids(3, 1)); // one side is enough
  CHECK_FALSE(list.avoids(1, 4));
}

TEST_CASE("avoid list: removing keeps the slot's cooldown")
{
  AvoidList list;
  REQUIRE(list.add(1, 2, 1002, T0) == AvoidList::AddResult::Added);
  REQUIRE(list.add(1, 3, 1003, T0) == AvoidList::AddResult::Added);
  CHECK(list.remove(1, 2));
  CHECK_FALSE(list.avoids(1, 2));
  CHECK(list.add(1, 4, 1004, T0 + Hour) == AvoidList::AddResult::NoFreeSlot); // emptied, but still cooling down
  CHECK(list.cooldownLeft(1, 0, T0 + Hour) == 5 * Hour);
  CHECK(list.add(1, 4, 1004, T0 + 6 * Hour) == AvoidList::AddResult::Added); // cooldown over
  CHECK(list.avoids(1, 4));
  CHECK(list.avoids(1, 3)); // the other slot still holds its player
}

TEST_CASE("avoid list: a filled slot stays filled after its cooldown")
{
  AvoidList list;
  REQUIRE(list.add(1, 2, 1002, T0) == AvoidList::AddResult::Added);
  REQUIRE(list.add(1, 3, 1003, T0) == AvoidList::AddResult::Added);
  CHECK(list.add(1, 4, 1004, T0 + 7 * Hour) == AvoidList::AddResult::NoFreeSlot);
  CHECK(list.cooldownLeft(1, 0, T0 + 7 * Hour) == 0);
}

TEST_CASE("avoid list: no self, no duplicates, remove of an unknown player fails")
{
  AvoidList list;
  CHECK(list.add(1, 1, 1001, T0) == AvoidList::AddResult::Self);
  REQUIRE(list.add(1, 2, 1002, T0) == AvoidList::AddResult::Added);
  CHECK(list.add(1, 2, 1002, T0) == AvoidList::AddResult::AlreadyAvoided);
  CHECK_FALSE(list.remove(1, 9));
  CHECK_FALSE(list.remove(7, 2));
}

TEST_CASE("avoid list: slots match accounts, the named character is display-only")
{
  AvoidList list;
  REQUIRE(list.add(7, 42, 1234, T0) == AvoidList::AddResult::Added);

  CHECK(list.avoids(7, 42));
  CHECK(list.slots(7)[0].avoidedGuid == 1234);
  CHECK_FALSE(list.avoids(7, 1234)); // the character guid is not the matching key
}

TEST_CASE("recent teammates: newest first, at most ten, no duplicates")
{
  RecentTeammates recent;
  for (uint32_t i = 0; i < 6; ++i)
    recent.record({RecentTeammates::Mate{1, "Me"}, {100 + 2 * i, "A"}, {101 + 2 * i, "B"}});
  auto mates = recent.of(1);
  CHECK(mates.size() == RecentTeammates::Kept);
  CHECK(mates.front().guid == 111); // last match, recorded last
  recent.record({RecentTeammates::Mate{1, "Me"}, {110, "A"}, {200, "C"}});
  mates = recent.of(1);
  CHECK(mates.size() == RecentTeammates::Kept);
  CHECK(mates[0].guid == 200);
  CHECK(mates[1].guid == 110); // moved to the front, not duplicated
  CHECK(std::count_if(mates.begin(), mates.end(), [](auto const& mate) { return mate.guid == 110; }) == 1);
  CHECK(recent.of(110).front().guid == 200);
}

TEST_CASE("cooldown text")
{
  CHECK(formatCooldown(0) == "now");
  CHECK(formatCooldown(30) == "1m");
  CHECK(formatCooldown(45 * 60) == "45m");
  CHECK(formatCooldown(3 * Hour + 12 * 60) == "3h 12m");
}
