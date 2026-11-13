#include <doctest/doctest.h>

#include "OmenOfClarity.hpp"

using arenacraft::ooc::procsAtWeaponSpeed;
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

TEST_CASE("omen of clarity: instants are 3.5 ppm on 1.5s (8.75%) minus a 33% fail chance = 5.86%")
{
  CHECK(spellProcChance(0, SpellKind::Instant) == doctest::Approx(5.8625));
  CHECK(spellProcChance(0, SpellKind::Instant) < spellProcChance(1500, SpellKind::CastTime));
}

TEST_CASE("omen of clarity: multi-target spells and hurricane ticks are heavily reduced")
{
  CHECK(spellProcChance(0, SpellKind::MultiTarget) == doctest::Approx(11.25 * 0.25));
  CHECK(spellProcChance(0, SpellKind::HurricaneTick) == doctest::Approx(11.25 * 2.0 / 9.0));
}

TEST_CASE("omen of clarity: no spell reaches the old ~20% weapon-speed chance of a staff")
{
  CHECK(spellProcChance(3500, SpellKind::CastTime) < 20.0f);
}

TEST_CASE("omen of clarity: shred, mangle and feral charge proc at the weapon-speed rate")
{
  CHECK(procsAtWeaponSpeed(5221));        // Shred
  CHECK(procsAtWeaponSpeed(33876));       // Mangle (Cat)
  CHECK(procsAtWeaponSpeed(33878));       // Mangle (Bear)
  CHECK(procsAtWeaponSpeed(16979));       // Feral Charge (Bear)
  CHECK(procsAtWeaponSpeed(49376));       // Feral Charge (Cat)
  CHECK_FALSE(procsAtWeaponSpeed(6807));  // Maul goes through the on-next-swing path
  CHECK_FALSE(procsAtWeaponSpeed(22568)); // Ferocious Bite
}
