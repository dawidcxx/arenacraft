#include <doctest/doctest.h>

#include "OmenOfClarity.hpp"

using arenacraft::ooc::isExtraMeleeProcSpell;
using arenacraft::ooc::SpellKind;
using arenacraft::ooc::spellProcChance;

TEST_CASE("omen of clarity: a 1.5s cast is 4.5 ppm (11.25%) minus the 33% spell fail chance")
{
  CHECK(spellProcChance(1500, SpellKind::CastTime) == doctest::Approx(11.25 * 2.0 / 3.0));
}

TEST_CASE("omen of clarity: longer casts proc more often, shorter (hasted) casts count as 1.5s")
{
  CHECK(spellProcChance(2500, SpellKind::CastTime) == doctest::Approx(2.5 * 4.5 / 60.0 * 100.0 * 2.0 / 3.0));
  CHECK(spellProcChance(1100, SpellKind::CastTime) == doctest::Approx(spellProcChance(1500, SpellKind::CastTime)));
}

TEST_CASE("omen of clarity: instants, multi-target spells and hurricane ticks are heavily reduced")
{
  CHECK(spellProcChance(0, SpellKind::Instant) == doctest::Approx(11.25 * 0.076));
  CHECK(spellProcChance(0, SpellKind::MultiTarget) == doctest::Approx(11.25 * 0.25));
  CHECK(spellProcChance(0, SpellKind::HurricaneTick) == doctest::Approx(11.25 * 2.0 / 9.0));
}

TEST_CASE("omen of clarity: no spell reaches the old ~20% weapon-speed chance of a staff")
{
  CHECK(spellProcChance(3500, SpellKind::CastTime) < 20.0f);
}

TEST_CASE("omen of clarity: shred and mangle are extra melee proc abilities")
{
  CHECK(isExtraMeleeProcSpell(5221));  // Shred
  CHECK(isExtraMeleeProcSpell(33876)); // Mangle (Cat)
  CHECK(isExtraMeleeProcSpell(33878)); // Mangle (Bear)
  CHECK_FALSE(isExtraMeleeProcSpell(6807));  // Maul goes through the on-next-swing path
  CHECK_FALSE(isExtraMeleeProcSpell(22568)); // Ferocious Bite
}
