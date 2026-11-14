#include <doctest/doctest.h>

#include "ProcRolls.hpp"

#include <vector>

using arenacraft::SharedProcRolls;

namespace
{
// Hands out the given values in order and counts how many rolls were made.
struct FakeRandom
{
  std::vector<float> values;
  std::size_t        used = 0;
  float              operator()() { return values.at(used++); }
};

int owner1 = 0, owner2 = 0;
} // namespace

TEST_CASE("shared proc rolls: procs of one unit with the same chance share one roll")
{
  FakeRandom      random{{10.0f, 99.0f}};
  SharedProcRolls rolls([&] { return random(); });

  CHECK(rolls.roll(&owner1, 15.0f)); // Frostbite 3/3
  CHECK(rolls.roll(&owner1, 15.0f)); // Fingers of Frost 2/2 - same roll
  CHECK(random.used == 1);
}

TEST_CASE("shared proc rolls: a shared miss misses for all of them")
{
  FakeRandom      random{{50.0f}};
  SharedProcRolls rolls([&] { return random(); });
  CHECK_FALSE(rolls.roll(&owner1, 15.0f));
  CHECK_FALSE(rolls.roll(&owner1, 15.0f));
}

TEST_CASE("shared proc rolls: different chances and different units roll on their own")
{
  FakeRandom      random{{10.0f, 12.0f, 90.0f}};
  SharedProcRolls rolls([&] { return random(); });

  CHECK(rolls.roll(&owner1, 15.0f));
  CHECK_FALSE(rolls.roll(&owner1, 10.0f)); // own roll: 12 >= 10
  CHECK_FALSE(rolls.roll(&owner2, 15.0f)); // another unit: own roll 90
  CHECK(random.used == 3);
}

TEST_CASE("shared proc rolls: 0% never and 100% always proc without using a roll")
{
  FakeRandom      random{};
  SharedProcRolls rolls([&] { return random(); });
  CHECK_FALSE(rolls.roll(&owner1, 0.0f));
  CHECK(rolls.roll(&owner1, 100.0f));
  CHECK(random.used == 0);
}
