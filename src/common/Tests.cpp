/*
 * Unit test entrypoint for the common module.
 *
 * This is the only translation unit that defines
 * DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN. Test cases live in co-located
 * `*_test.cpp` files under src/common/ and are picked up by the build harness.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
