#include "TourneySchedule.hpp"

#include "Timer.h"

namespace arenacraft::soloq
{
bool isTourneyTime(std::tm const& localTime)
{
  switch (localTime.tm_wday)
  {
  case 6: // Saturday: starts at 00:01
    return localTime.tm_hour > 0 || localTime.tm_min >= 1;
  case 0: // Sunday: the whole day, through 23:59
    return true;
  default:
    return false;
  }
}

bool isTourneyTimeNow() { return isTourneyTime(Acore::Time::TimeBreakdown()); }
} // namespace arenacraft::soloq
