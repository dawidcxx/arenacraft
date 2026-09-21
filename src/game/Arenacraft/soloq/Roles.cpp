#include "Roles.hpp"

namespace arenacraft::soloq
{
namespace
{
std::optional<Role> fromSpec(uint8_t specIndex, Role first, Role second, Role third)
{
  switch (specIndex)
  {
  case 0:
    return first;
  case 1:
    return second;
  case 2:
    return third;
  default:
    return std::nullopt;
  }
}
} // namespace

std::optional<Role> roleFor(Classes classId, uint8_t specIndex)
{
  switch (classId)
  {
  case CLASS_WARRIOR:
    return fromSpec(specIndex, Role::Melee, Role::Melee, Role::Melee);
  case CLASS_PALADIN:
    return fromSpec(specIndex, Role::Healer, Role::Melee, Role::Melee);
  case CLASS_HUNTER:
    return fromSpec(specIndex, Role::Melee, Role::Melee, Role::Melee);
  case CLASS_ROGUE:
    return fromSpec(specIndex, Role::Melee, Role::Melee, Role::Melee);
  case CLASS_PRIEST:
    return fromSpec(specIndex, Role::Healer, Role::Healer, Role::Caster);
  case CLASS_DEATH_KNIGHT:
    return fromSpec(specIndex, Role::Melee, Role::Melee, Role::Melee);
  case CLASS_SHAMAN:
    return fromSpec(specIndex, Role::Caster, Role::Melee, Role::Healer);
  case CLASS_MAGE:
    return fromSpec(specIndex, Role::Caster, Role::Caster, Role::Caster);
  case CLASS_WARLOCK:
    return fromSpec(specIndex, Role::Caster, Role::Caster, Role::Caster);
  case CLASS_DRUID:
    return fromSpec(specIndex, Role::Caster, Role::Melee, Role::Healer);
  default:
    return std::nullopt;
  }
}
} // namespace arenacraft::soloq
