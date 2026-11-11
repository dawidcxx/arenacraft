#include <doctest/doctest.h>

#include "TourneySchedule.hpp"

#include <ctime>

using arenacraft::soloq::isTourneyTime;

namespace
{
std::tm at(int weekday, int hour, int minute)
{
  std::tm local{};
  local.tm_wday = weekday; // 0 = Sunday .. 6 = Saturday
  local.tm_hour = hour;
  local.tm_min  = minute;
  return local;
}
} // namespace

TEST_CASE("tourney window: Saturday starts at 00:01")
{
  CHECK_FALSE(isTourneyTime(at(6, 0, 0)));
  CHECK(isTourneyTime(at(6, 0, 1)));
  CHECK(isTourneyTime(at(6, 12, 0)));
  CHECK(isTourneyTime(at(6, 23, 59)));
}

TEST_CASE("tourney window: the whole of Sunday")
{
  CHECK(isTourneyTime(at(0, 0, 0)));
  CHECK(isTourneyTime(at(0, 12, 0)));
  CHECK(isTourneyTime(at(0, 23, 59)));
}

TEST_CASE("tourney window: off on the other days")
{
  CHECK_FALSE(isTourneyTime(at(5, 23, 59))); // Friday
  CHECK_FALSE(isTourneyTime(at(1, 0, 0)));   // Monday
  CHECK_FALSE(isTourneyTime(at(3, 12, 0)));  // Wednesday
}
