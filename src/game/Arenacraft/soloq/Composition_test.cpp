#include <doctest/doctest.h>

#include "Composition.hpp"

using arenacraft::soloq::classAbbreviation;
using arenacraft::soloq::formatTeamComposition;
using arenacraft::soloq::specName;
using SoloqTeam = arenacraft::soloq::Team;

namespace
{
arenacraft::soloq::QueuedPlayer makePlayer(Classes classId, uint8_t specIndex)
{
  arenacraft::soloq::QueuedPlayer player{};
  player.classId   = classId;
  player.specIndex = specIndex;
  return player;
}
} // namespace

TEST_CASE("classAbbreviation uses the SoloQ display names")
{
  CHECK(classAbbreviation(CLASS_WARRIOR) == "War");
  CHECK(classAbbreviation(CLASS_PALADIN) == "Pala");
  CHECK(classAbbreviation(CLASS_HUNTER) == "Hunt");
  CHECK(classAbbreviation(CLASS_ROGUE) == "Rogue");
  CHECK(classAbbreviation(CLASS_PRIEST) == "Priest");
  CHECK(classAbbreviation(CLASS_DEATH_KNIGHT) == "DK");
  CHECK(classAbbreviation(CLASS_SHAMAN) == "Sham");
  CHECK(classAbbreviation(CLASS_MAGE) == "Mage");
  CHECK(classAbbreviation(CLASS_WARLOCK) == "Lock");
  CHECK(classAbbreviation(CLASS_DRUID) == "Druid");
  CHECK(classAbbreviation(CLASS_NONE) == "Unknown");
}

TEST_CASE("specName maps every tree to its short spec name")
{
  CHECK(specName(CLASS_WARRIOR, 0) == "Arms");
  CHECK(specName(CLASS_WARRIOR, 2) == "Prot");
  CHECK(specName(CLASS_PALADIN, 0) == "Holy");
  CHECK(specName(CLASS_PALADIN, 2) == "Ret");
  CHECK(specName(CLASS_PRIEST, 2) == "Shadow");
  CHECK(specName(CLASS_DEATH_KNIGHT, 0) == "Blood");
  CHECK(specName(CLASS_SHAMAN, 0) == "Ele");
  CHECK(specName(CLASS_SHAMAN, 2) == "Resto");
  CHECK(specName(CLASS_MAGE, 1) == "Fire");
  CHECK(specName(CLASS_WARLOCK, 1) == "Demo");
  CHECK(specName(CLASS_DRUID, 1) == "Feral");
  CHECK(specName(CLASS_WARRIOR, 3) == "Unknown");
  CHECK(specName(CLASS_NONE, 0) == "Unknown");
}

TEST_CASE("formatTeamComposition lists healer, melee and caster with role labels")
{
  SoloqTeam team{};
  team.healer = makePlayer(CLASS_PALADIN, 0);
  team.melee  = makePlayer(CLASS_WARRIOR, 0);
  team.caster = makePlayer(CLASS_SHAMAN, 0);

  CHECK(formatTeamComposition(team) == "Holy Pala (Healer), Arms War (Melee), Ele Sham (Caster)");
}

TEST_CASE("formatTeamComposition derives the role from class and spec, not the slot")
{
  SoloqTeam team{};
  team.healer = makePlayer(CLASS_DRUID, 2);
  team.melee  = makePlayer(CLASS_DEATH_KNIGHT, 2);
  team.caster = makePlayer(CLASS_HUNTER, 1);

  CHECK(formatTeamComposition(team) == "Resto Druid (Healer), Unholy DK (Melee), MM Hunt (Melee)");
}
