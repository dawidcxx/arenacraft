#pragma once

#include <cstdint>

// Omen of Clarity (druid talent 16864) proc chance on spells. The DB rate is 3.5 procs per minute measured on the
// druid's weapon speed, which turns a 3.3-3.6s staff into a ~20% chance on every spell. Blizzard described the WotLK
// behaviour (WotLK Classic, quoted in azerothcore-wotlk#24395) as 3.5 PPM measured on the spell's cast time (at least
// 1.5s), followed by a fail chance per kind of spell. Auto attacks and melee abilities keep the weapon-speed PPM.
// Arenacraft tunes the spell rate above Blizzard's 3.5 PPM (players found the original 3.5 too weak); the per-kind
// fail chances are unchanged, so every spell kind scales together.
namespace arenacraft::ooc
{
enum class SpellKind : uint8_t
{
  CastTime,      // spell with a cast time: 33% fail
  Instant,       // instant (or channeled): 92.4% fail
  MultiTarget,   // Wild Growth, Typhoon - rolled once per target: 75% fail
  HurricaneTick, // Hurricane damage event: 7/9 fail
};

inline constexpr float    ProcsPerMinute = 4.5f;
inline constexpr uint32_t MinCastTimeMs  = 1500;

// Chance in percent (0..100) for one proc roll.
float spellProcChance(uint32_t castTimeMs, SpellKind kind);

// Abilities that also proc Omen of Clarity on top of auto attacks and on-next-swing attacks (Maul). They keep the DB
// weapon-speed PPM instead of the spell chance: 5% per Shred/Mangle/Feral Charge in cat form (1.0s attack time), 14%
// per bear Mangle/Feral Charge (2.5s).
bool procsAtWeaponSpeed(uint32_t firstRankSpellId);
} // namespace arenacraft::ooc
