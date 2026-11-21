#include <doctest/doctest.h>

#include "Solo2v2Rules.hpp"

using namespace arenacraft::solo2v2;

TEST_CASE("solo 2v2 team name follows the SoloQ naming") { CHECK(teamNameFor("Arthas") == "Arthas's 2v2"); }

TEST_CASE("party MMR gap of exactly 300 is allowed, 301 is not")
{
  CHECK(partyMmrGapAllowed(1500, 1800));
  CHECK(partyMmrGapAllowed(1800, 1500));
  CHECK_FALSE(partyMmrGapAllowed(1500, 1801));
  CHECK_FALSE(partyMmrGapAllowed(2200, 1500));
}

TEST_CASE("party MMR is the mean of both members") { CHECK(partyMmr(1500, 1800) == 1650); }
