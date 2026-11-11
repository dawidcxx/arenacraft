#pragma once

#include <ctime>

namespace arenacraft::soloq
{
// The weekend tourney window: Saturday 00:01 through Sunday 23:59, in server local time. Pure calendar logic, so it
// is unit-tested without touching the clock.
[[nodiscard]] bool isTourneyTime(std::tm const& localTime);

// Same check against the host's local calendar right now. This calls localtime_r, so callers should throttle it (the
// world driver refreshes it at most every 10 minutes) instead of running it per tick.
[[nodiscard]] bool isTourneyTimeNow();
} // namespace arenacraft::soloq
