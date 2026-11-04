#include <doctest/doctest.h>

#include "OmenOfClarity.hpp"

using arenacraft::ooc::SpellKind;
using arenacraft::ooc::spellProcChance;

TEST_CASE("omen of clarity: a 1.5s cast is 3.5 ppm (8.75%) minus the 33% spell fail chance")
{
  CHECK(spellProcChance(1500, SpellKind::CastTime) == doctest::Approx(8.75 * 2.0 / 3.0));
}

TEST_CASE("omen of clarity: longer casts proc more often, shorter (hasted) casts count as 1.5s")
{
  CHECK(spellProcChance(2500, SpellKind::CastTime) == doctest::Approx(2.5 * 3.5 / 60.0 * 100.0 * 2.0 / 3.0));
  CHECK(spellProcChance(1100, SpellKind::CastTime) == doctest::Approx(spellProcChance(1500, SpellKind::CastTime)));
}

TEST_CASE("omen of clarity: instants, multi-target spells and hurricane ticks are heavily reduced")
{
  CHECK(spellProcChance(0, SpellKind::Instant) == doctest::Approx(8.75 * 0.076));
  CHECK(spellProcChance(0, SpellKind::MultiTarget) == doctest::Approx(8.75 * 0.25));
  CHECK(spellProcChance(0, SpellKind::HurricaneTick) == doctest::Approx(8.75 * 2.0 / 9.0));
}

TEST_CASE("omen of clarity: no spell comes near the old ~20% weapon-speed chance of a staff")
{
  CHECK(spellProcChance(3500, SpellKind::CastTime) < 15.0f);
}
