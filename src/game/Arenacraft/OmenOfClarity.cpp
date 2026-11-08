#include "OmenOfClarity.hpp"

#include <algorithm>

namespace arenacraft::ooc
{
float spellProcChance(uint32_t castTimeMs, SpellKind kind)
{
  // instants and channels are measured like a 1.5s cast
  uint32_t const measuredMs = kind == SpellKind::CastTime ? std::max(castTimeMs, MinCastTimeMs) : MinCastTimeMs;
  float const    ppmChance  = measuredMs / 1000.0f * ProcsPerMinute / 60.0f * 100.0f;

  float keep = 1.0f;
  switch (kind)
  {
  case SpellKind::CastTime:
    keep = 2.0f / 3.0f;
    break;
  case SpellKind::Instant:
    keep = 1.0f - 0.924f;
    break;
  case SpellKind::MultiTarget:
    keep = 0.25f;
    break;
  case SpellKind::HurricaneTick:
    keep = 2.0f / 9.0f;
    break;
  }
  return ppmChance * keep;
}

bool procsAtWeaponSpeed(uint32_t firstRankSpellId)
{
  switch (firstRankSpellId)
  {
  case 5221:  // Shred
  case 33876: // Mangle (Cat)
  case 33878: // Mangle (Bear)
  case 16979: // Feral Charge (Bear)
  case 49376: // Feral Charge (Cat)
    return true;
  default:
    return false;
  }
}
} // namespace arenacraft::ooc
