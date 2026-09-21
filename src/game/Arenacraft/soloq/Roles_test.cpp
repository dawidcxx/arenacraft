#include <doctest/doctest.h>

#include "Roles.hpp"

using arenacraft::soloq::Role;
using arenacraft::soloq::roleFor;

namespace
{
struct RoleCase
{
  Classes classId;
  uint8_t specIndex;
  Role    expected;
};
} // namespace

TEST_CASE("roleFor maps each class and spec to its role")
{
  RoleCase const cases[] = {
      {CLASS_WARRIOR, 0, Role::Melee},      {CLASS_WARRIOR, 1, Role::Melee},      {CLASS_WARRIOR, 2, Role::Melee},
      {CLASS_PALADIN, 0, Role::Healer},     {CLASS_PALADIN, 1, Role::Melee},      {CLASS_PALADIN, 2, Role::Melee},
      {CLASS_HUNTER, 0, Role::Melee},       {CLASS_HUNTER, 1, Role::Melee},       {CLASS_HUNTER, 2, Role::Melee},
      {CLASS_ROGUE, 0, Role::Melee},        {CLASS_ROGUE, 1, Role::Melee},        {CLASS_ROGUE, 2, Role::Melee},
      {CLASS_PRIEST, 0, Role::Healer},      {CLASS_PRIEST, 1, Role::Healer},      {CLASS_PRIEST, 2, Role::Caster},
      {CLASS_DEATH_KNIGHT, 0, Role::Melee}, {CLASS_DEATH_KNIGHT, 1, Role::Melee}, {CLASS_DEATH_KNIGHT, 2, Role::Melee},
      {CLASS_SHAMAN, 0, Role::Caster},      {CLASS_SHAMAN, 1, Role::Melee},       {CLASS_SHAMAN, 2, Role::Healer},
      {CLASS_MAGE, 0, Role::Caster},        {CLASS_MAGE, 1, Role::Caster},        {CLASS_MAGE, 2, Role::Caster},
      {CLASS_WARLOCK, 0, Role::Caster},     {CLASS_WARLOCK, 1, Role::Caster},     {CLASS_WARLOCK, 2, Role::Caster},
      {CLASS_DRUID, 0, Role::Caster},       {CLASS_DRUID, 1, Role::Melee},        {CLASS_DRUID, 2, Role::Healer},
  };

  for (RoleCase const& roleCase : cases)
  {
    INFO("class " << static_cast<int>(roleCase.classId) << ", spec " << static_cast<int>(roleCase.specIndex));
    std::optional<Role> const role = roleFor(roleCase.classId, roleCase.specIndex);
    REQUIRE(role.has_value());
    CHECK(*role == roleCase.expected);
  }
}

TEST_CASE("roleFor rejects classes that are not playable")
{
  CHECK_FALSE(roleFor(CLASS_NONE, 0).has_value());
  CHECK_FALSE(roleFor(static_cast<Classes>(10), 0).has_value());
  CHECK_FALSE(roleFor(static_cast<Classes>(42), 0).has_value());
}

TEST_CASE("roleFor rejects spec indexes outside the three talent trees")
{
  Classes const playable[] = {
      CLASS_WARRIOR,      CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,   CLASS_PRIEST,
      CLASS_DEATH_KNIGHT, CLASS_SHAMAN,  CLASS_MAGE,   CLASS_WARLOCK, CLASS_DRUID,
  };

  for (Classes const classId : playable)
  {
    INFO("class " << static_cast<int>(classId));
    CHECK_FALSE(roleFor(classId, 3).has_value());
    CHECK_FALSE(roleFor(classId, 255).has_value());
  }
}
