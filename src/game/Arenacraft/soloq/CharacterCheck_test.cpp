#include <doctest/doctest.h>

#include "CharacterCheck.hpp"

#include <cstdint>

using arenacraft::soloq::CharacterProblem;
using arenacraft::soloq::CharacterSnapshot;
using arenacraft::soloq::checkCharacter;
using arenacraft::soloq::EquippedItemInfo;
using arenacraft::soloq::RequiredEnchantedSlots;
using arenacraft::soloq::RequiredEquipmentSlots;

namespace
{
CharacterSnapshot readyCharacter()
{
  CharacterSnapshot snapshot;
  snapshot.glyphSlotsEnabled = (1u << MAX_GLYPH_SLOT_INDEX) - 1;
  snapshot.glyphs.fill(true);

  for (uint8_t const slot : RequiredEquipmentSlots)
    snapshot.equipment[slot] = EquippedItemInfo{};

  for (uint8_t const slot : RequiredEnchantedSlots)
    snapshot.equipment[slot]->enchanted = true;

  return snapshot;
}
} // namespace

TEST_CASE("checkCharacter accepts a fully finished character") { CHECK_FALSE(checkCharacter(readyCharacter())); }

TEST_CASE("checkCharacter rejects a character with unspent talents")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.freeTalentPoints  = 3;

  CHECK(checkCharacter(snapshot) == CharacterProblem::UnspentTalents);
}

TEST_CASE("checkCharacter rejects a talent learned without its max rank")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.knownSpells.insert(47540); // Penance rank 1

  CHECK(checkCharacter(snapshot) == CharacterProblem::UnmaxedTalent);
}

TEST_CASE("checkCharacter accepts a talent learned at its max rank")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.knownSpells.insert(47540); // Penance rank 1
  snapshot.knownSpells.insert(53007); // Penance max rank

  CHECK_FALSE(checkCharacter(snapshot));
}

TEST_CASE("checkCharacter ignores a max rank learned without the first rank")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.knownSpells.insert(53007); // Penance max rank without the talent

  CHECK_FALSE(checkCharacter(snapshot));
}

TEST_CASE("checkCharacter rejects any empty required gear slot")
{
  for (uint8_t const slot : RequiredEquipmentSlots)
  {
    CharacterSnapshot snapshot = readyCharacter();
    snapshot.equipment[slot].reset();

    CHECK(checkCharacter(snapshot) == CharacterProblem::MissingEquipment);
  }
}

TEST_CASE("checkCharacter ignores cosmetic and optional slots")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.equipment[EQUIPMENT_SLOT_BODY].reset();
  snapshot.equipment[EQUIPMENT_SLOT_TABARD].reset();
  snapshot.equipment[EQUIPMENT_SLOT_OFFHAND].reset();
  snapshot.equipment[EQUIPMENT_SLOT_RANGED].reset();

  CHECK_FALSE(checkCharacter(snapshot));
}

TEST_CASE("checkCharacter rejects any of the mandatory enchant slots")
{
  for (uint8_t const slot : RequiredEnchantedSlots)
  {
    CharacterSnapshot snapshot          = readyCharacter();
    snapshot.equipment[slot]->enchanted = false;

    CHECK(checkCharacter(snapshot) == CharacterProblem::MissingEnchant);
  }
}

TEST_CASE("checkCharacter rejects an item with an empty gem socket")
{
  CharacterSnapshot snapshot               = readyCharacter();
  snapshot.equipment[EQUIPMENT_SLOT_CHEST] = EquippedItemInfo{2, 1, true};

  CHECK(checkCharacter(snapshot) == CharacterProblem::EmptySocket);
}

TEST_CASE("checkCharacter accepts items whose sockets are all filled")
{
  CharacterSnapshot snapshot               = readyCharacter();
  snapshot.equipment[EQUIPMENT_SLOT_CHEST] = EquippedItemInfo{3, 3, true};
  snapshot.equipment[EQUIPMENT_SLOT_LEGS]  = EquippedItemInfo{1, 1, true};

  CHECK_FALSE(checkCharacter(snapshot));
}

TEST_CASE("checkCharacter rejects every enabled but empty glyph slot")
{
  for (uint8_t slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
  {
    CharacterSnapshot snapshot = readyCharacter();
    snapshot.glyphs[slot]      = false;

    CHECK(checkCharacter(snapshot) == CharacterProblem::EmptyGlyphSlot);
  }
}

TEST_CASE("checkCharacter ignores glyph slots that are not enabled")
{
  CharacterSnapshot snapshot = readyCharacter();
  snapshot.glyphSlotsEnabled = (1u << MAX_GLYPH_SLOT_INDEX) - 2; // clear bit 0
  snapshot.glyphs[0]         = false;

  CHECK_FALSE(checkCharacter(snapshot));
}
