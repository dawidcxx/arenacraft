#include "CharacterCheck.hpp"

#include "Item.h"

namespace arenacraft::soloq
{
std::optional<CharacterProblem> checkCharacter(CharacterSnapshot const& snapshot)
{
  if (snapshot.freeTalentPoints > 0)
    return CharacterProblem::UnspentTalents;

  for (uint8_t const slot : RequiredEquipmentSlots)
    if (!snapshot.equipment[slot])
      return CharacterProblem::MissingEquipment;

  for (uint8_t const slot : RequiredEnchantedSlots)
    if (snapshot.equipment[slot] && !snapshot.equipment[slot]->enchanted)
      return CharacterProblem::MissingEnchant;

  for (std::optional<EquippedItemInfo> const& item : snapshot.equipment)
    if (item && item->filled < item->sockets)
      return CharacterProblem::EmptySocket;

  for (uint8_t slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    if ((snapshot.glyphSlotsEnabled & (1u << slot)) && !snapshot.glyphs[slot])
      return CharacterProblem::EmptyGlyphSlot;

  return std::nullopt;
}

char const* describeCharacterProblem(CharacterProblem problem)
{
  switch (problem)
  {
  case CharacterProblem::UnspentTalents:
    return "spend all your talent points before queueing.";
  case CharacterProblem::MissingEquipment:
    return "equip an item in every gear slot before queueing.";
  case CharacterProblem::MissingEnchant:
    return "Your character is missing enchantments!";
  case CharacterProblem::EmptySocket:
    return "fill every gem socket on your equipped items before queueing.";
  case CharacterProblem::EmptyGlyphSlot:
    return "fill every glyph slot before queueing.";
  }

  return "your character is not ready to queue.";
}

CharacterSnapshot snapshotCharacter(Player* player)
{
  CharacterSnapshot snapshot;
  if (!player)
    return snapshot;

  snapshot.freeTalentPoints  = player->GetFreeTalentPoints();
  snapshot.glyphSlotsEnabled = player->GetUInt32Value(PLAYER_GLYPHS_ENABLED);

  for (uint8_t slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    snapshot.glyphs[slot] = player->GetGlyph(slot) != 0;

  for (uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
  {
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item)
      continue;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
      continue;

    EquippedItemInfo info;
    info.enchanted = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) != 0;

    for (uint8_t socket = 0; socket < MAX_ITEM_PROTO_SOCKETS; ++socket)
    {
      if (!proto->Socket[socket].Color)
        continue;

      ++info.sockets;
      if (item->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + socket)))
        ++info.filled;
    }

    snapshot.equipment[slot] = info;
  }

  return snapshot;
}
} // namespace arenacraft::soloq
