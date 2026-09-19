/*
 * Unit test entrypoint for the game module.
 *
 * This is the only translation unit that defines
 * DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN. Test cases live in co-located
 * `*_test.cpp` files under src/game/ and are picked up by the build harness.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("sanity: arithmetic still works")
{
  CHECK(2 + 2 == 4);
  CHECK(2 * 2 != 5);
}
