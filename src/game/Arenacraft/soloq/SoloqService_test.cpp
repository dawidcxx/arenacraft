#include <doctest/doctest.h>

#include "SoloqService.hpp"

using arenacraft::soloq::formatQueueStats;
using arenacraft::soloq::RoleCounts;

TEST_CASE("formatQueueStats renders the horde line before the alliance line")
{
  RoleCounts const horde{1, 5, 0};
  RoleCounts const alliance{2, 0, 5};
  CHECK(formatQueueStats(horde, alliance) == "SoloQ Queue Status\n-----------------------\n\n"
                                             "[H]: Melee (1) Caster (5) Healer (0)\n\n"
                                             "[A]: Melee (2) Caster (0) Healer (5)");
}

TEST_CASE("formatQueueStats renders empty queues as zeros")
{
  RoleCounts const empty{};
  CHECK(formatQueueStats(empty, empty) == "SoloQ Queue Status\n-----------------------\n\n"
                                          "[H]: Melee (0) Caster (0) Healer (0)\n\n"
                                          "[A]: Melee (0) Caster (0) Healer (0)");
}
