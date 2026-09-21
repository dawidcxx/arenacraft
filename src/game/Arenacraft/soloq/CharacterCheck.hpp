#pragma once

#include "Player.h"

#include <array>
#include <cstdint>
#include <optional>

namespace arenacraft::soloq
{
// Why a character cannot queue yet. This is an anti-footgun gate for queueing
// with an obviously unfinished character, not cheat prevention: the result is
// cached, so gear/talent changes after a successful check are ignored.
enum class CharacterProblem : uint8_t
{
  UnspentTalents,
  MissingEquipment,
  MissingEnchant,
  EmptySocket,
  EmptyGlyphSlot
};

// What the check needs to know about a single equipped item. `sockets` counts
// the item's built-in sockets; `filled` counts those holding a gem; `enchanted`
// is true when the item has a permanent enchant. Optional prismatic sockets are
// not counted.
struct EquippedItemInfo
{
  uint8_t sockets   = 0;
  uint8_t filled    = 0;
  bool    enchanted = false;
};

// Offline snapshot of everything the check needs, indexed by core
// EquipmentSlots, so the logic stays unit-testable without a live Player.
struct CharacterSnapshot
{
  uint32_t                                                        freeTalentPoints  = 0;
  uint32_t                                                        glyphSlotsEnabled = 0;
  std::array<bool, MAX_GLYPH_SLOT_INDEX>                          glyphs{};
  std::array<std::optional<EquippedItemInfo>, EQUIPMENT_SLOT_END> equipment{};
};

// Slots a queueing character must have filled. Cosmetic slots (shirt, tabard)
// and the optional offhand/ranged slots are not required: two-handers and many
// casters legitimately leave those empty.
inline constexpr std::array<uint8_t, 15> RequiredEquipmentSlots = {
    EQUIPMENT_SLOT_HEAD,     EQUIPMENT_SLOT_NECK,    EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_CHEST,
    EQUIPMENT_SLOT_WAIST,    EQUIPMENT_SLOT_LEGS,    EQUIPMENT_SLOT_FEET,      EQUIPMENT_SLOT_WRISTS,
    EQUIPMENT_SLOT_HANDS,    EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2,   EQUIPMENT_SLOT_TRINKET1,
    EQUIPMENT_SLOT_TRINKET2, EQUIPMENT_SLOT_BACK,    EQUIPMENT_SLOT_MAINHAND};

// Slots that must carry a permanent enchant (arcanum/inscription/armor kit/
// spellthread etc.). All of these are also in RequiredEquipmentSlots.
inline constexpr std::array<uint8_t, 7> RequiredEnchantedSlots = {
    EQUIPMENT_SLOT_HEAD,  EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_CHEST, EQUIPMENT_SLOT_WRISTS,
    EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_LEGS,      EQUIPMENT_SLOT_FEET};

// Pure: returns the first problem found, or nullopt when the snapshot is ready.
// Checked in the order talents -> missing gear -> missing enchant -> empty
// gems -> empty glyphs.
std::optional<CharacterProblem> checkCharacter(CharacterSnapshot const& snapshot);

// Message fragment (no "SoloQ: " prefix) describing the problem.
char const* describeCharacterProblem(CharacterProblem problem);

// Core-facing: reads the live player's talents, equipment, enchants and glyphs.
CharacterSnapshot snapshotCharacter(Player* player);
} // namespace arenacraft::soloq
