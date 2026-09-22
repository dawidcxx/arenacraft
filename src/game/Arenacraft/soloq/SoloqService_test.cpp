#include <doctest/doctest.h>

#include "SoloqService.hpp"

using arenacraft::soloq::formatQueueStats;
using arenacraft::soloq::RoleCounts;

TEST_CASE("formatQueueStats renders the shared queue line with a total")
{
  RoleCounts const counts{1, 5, 0};
  CHECK(formatQueueStats(counts) == "SoloQ Queue\n-----------------------\n\nPlayers in queue: 6\n\n"
                                    "Melee (1) Caster (5) Healer (0)");
}

TEST_CASE("formatQueueStats renders an empty queue as zeros")
{
  RoleCounts const empty{};
  CHECK(formatQueueStats(empty) == "SoloQ Queue\n-----------------------\n\nPlayers in queue: 0\n\n"
                                   "Melee (0) Caster (0) Healer (0)");
}
