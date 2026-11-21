#include <doctest/doctest.h>

#include "SoloqMatchmaker.hpp"
#include "SoloqService.hpp"

using arenacraft::soloq::formatQueueStats;
using arenacraft::soloq::Match;
using arenacraft::soloq::modeName;
using arenacraft::soloq::parseMode;
using arenacraft::soloq::PlayerId;
using arenacraft::soloq::RoleCounts;
using arenacraft::soloq::SoloqMode;
using arenacraft::soloq::SoloqService;

TEST_CASE("formatQueueStats renders the shared queue block with mode and total")
{
  RoleCounts const counts{1, 5, 0};
  CHECK(formatQueueStats(counts, "flex") == "SoloQ Queue\n-----------------------\n\nPlayers in queue: 6\n"
                                            "Mode: flex\n\n"
                                            "Melee (1) Caster (5) Healer (0)");
}

TEST_CASE("formatQueueStats renders an empty queue as zeros")
{
  RoleCounts const empty{};
  CHECK(formatQueueStats(empty, "mch") == "SoloQ Queue\n-----------------------\n\nPlayers in queue: 0\n"
                                          "Mode: mch\n\n"
                                          "Melee (0) Caster (0) Healer (0)");
}

TEST_CASE("mode names round-trip through parseMode")
{
  CHECK(modeName(SoloqMode::Flex) == "flex");
  CHECK(modeName(SoloqMode::Mch) == "mch");

  CHECK(parseMode("flex") == SoloqMode::Flex);
  CHECK(parseMode("mch") == SoloqMode::Mch);
  CHECK(parseMode("MCH") == SoloqMode::Mch);
  CHECK(parseMode("Flex") == SoloqMode::Flex);
  CHECK_FALSE(parseMode("standard").has_value());
  CHECK_FALSE(parseMode("").has_value());
}

TEST_CASE("SoloqService::setMode keeps the queue and pending arenas")
{
  SoloqService& service = SoloqService::instance();
  service.setMode(SoloqMode::Flex); // start from the default mode

  for (PlayerId id = 1; id <= 6; ++id)
  {
    Classes const classId = id <= 2 ? CLASS_WARRIOR : id <= 4 ? CLASS_MAGE : CLASS_PRIEST;
    uint8_t const spec    = id <= 4 ? 0 : 1;
    REQUIRE(service.join(id, static_cast<uint32_t>(id), classId, spec, TEAM_ALLIANCE, 1400, 1500));
  }
  REQUIRE(service.queueSize() == 6);

  Match pending{};
  service.registerMatch(4242, pending, 0);
  REQUIRE(service.pendingArena(4242).has_value());

  service.setMode(SoloqMode::Mch);
  CHECK(service.mode() == SoloqMode::Mch);
  CHECK(service.queueSize() == 6);
  CHECK(service.pendingArena(4242).has_value());

  // Leave the singleton clean for the rest of the test binary.
  for (PlayerId const id : service.waitingPlayers())
    service.leave(id);
  service.takeMatch(4242);
  service.setMode(SoloqMode::Flex);
}
