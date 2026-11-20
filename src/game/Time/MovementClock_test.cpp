#include <doctest/doctest.h>

#include "MovementClock.h"

TEST_CASE("MovementClock accepts a zero offset after initial synchronization")
{
  MovementClock clock;
  CHECK(clock.Synchronize(100, 500) == 500);
  clock.SetOffset(0);
  CHECK(clock.Synchronize(200, 600) == 200);
  CHECK(clock.Synchronize(300, 700) == 300);
}

TEST_CASE("MovementClock recovers gradually in either direction without sync-time jumps")
{
  for (int32 correction : {-100, 100})
  {
    MovementClock clock;
    clock.SetOffset(1000);
    CHECK(clock.Synchronize(1000, 2000) == 2000);
    clock.SetOffset(uint32(1000 + correction));
    CHECK(clock.Synchronize(1000, 2000) == 2000);

    uint32 previous = 2000;
    for (uint32 clientTime = 1010; clientTime <= 11000; clientTime += 10)
    {
      uint32 current = clock.Synchronize(clientTime, clientTime + 1000);
      CHECK(current >= previous + 9);
      CHECK(current <= previous + 11);
      previous = current;
    }
    CHECK(previous == uint32(12000 + correction));
  }
}

TEST_CASE("MovementClock handles independently wrapping client and server clocks")
{
  MovementClock clock;
  clock.SetOffset(200);
  CHECK(clock.Synchronize(0xFFFFFFF0u, 184) == 184);
  CHECK(clock.Synchronize(84, 284) == 284);

  clock.SetOffset(uint32(-200));
  CHECK(clock.Synchronize(184, 384) == 383);

  MovementClock serverWrap;
  serverWrap.SetOffset(uint32(-200));
  CHECK(serverWrap.Synchronize(190, 0xFFFFFFF6u) == 0xFFFFFFF6u);
  CHECK(serverWrap.Synchronize(210, 10) == 10);
  serverWrap.SetOffset(200);
  CHECK(serverWrap.Synchronize(310, 110) == 111);
}

TEST_CASE("MovementClock does not accrue catch-up while idle or from duplicate and older timestamps")
{
  MovementClock clock;
  clock.SetOffset(1000);
  CHECK(clock.Synchronize(1000, 2000) == 2000);
  CHECK(clock.Synchronize(60000, 61000) == 61000);
  clock.SetOffset(900);
  CHECK(clock.Synchronize(60000, 61000) == 61000);
  CHECK(clock.Synchronize(59900, 61000) == 60900);
  CHECK(clock.Synchronize(60010, 61010) == 61010);
  CHECK(clock.Synchronize(120000, 121000) == 120990);
}
