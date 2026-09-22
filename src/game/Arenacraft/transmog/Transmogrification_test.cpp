#include <doctest/doctest.h>

#include "Transmogrification.hpp"

using namespace arenacraft::transmog;

TEST_CASE("transmog: quality gate allows only rare, epic and legendary")
{
  CHECK_FALSE(IsTransmoggableQuality(ITEM_QUALITY_POOR));
  CHECK_FALSE(IsTransmoggableQuality(ITEM_QUALITY_NORMAL));
  CHECK_FALSE(IsTransmoggableQuality(ITEM_QUALITY_UNCOMMON));
  CHECK(IsTransmoggableQuality(ITEM_QUALITY_RARE));
  CHECK(IsTransmoggableQuality(ITEM_QUALITY_EPIC));
  CHECK(IsTransmoggableQuality(ITEM_QUALITY_LEGENDARY));
  CHECK_FALSE(IsTransmoggableQuality(ITEM_QUALITY_ARTIFACT));
  CHECK_FALSE(IsTransmoggableQuality(ITEM_QUALITY_HEIRLOOM));
}

TEST_CASE("transmog: inventory types map to equipment slots")
{
  CHECK(EquipmentSlotForInventoryType(INVTYPE_HEAD) == EQUIPMENT_SLOT_HEAD);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_SHOULDERS) == EQUIPMENT_SLOT_SHOULDERS);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_CHEST) == EQUIPMENT_SLOT_CHEST);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_ROBE) == EQUIPMENT_SLOT_CHEST);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_CLOAK) == EQUIPMENT_SLOT_BACK);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_WEAPON) == EQUIPMENT_SLOT_MAINHAND);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_2HWEAPON) == EQUIPMENT_SLOT_MAINHAND);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_WEAPONOFFHAND) == EQUIPMENT_SLOT_OFFHAND);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_SHIELD) == EQUIPMENT_SLOT_OFFHAND);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_RANGED) == EQUIPMENT_SLOT_RANGED);

  // Slots that must never be transmogged.
  CHECK(EquipmentSlotForInventoryType(INVTYPE_NECK) == EQUIPMENT_SLOT_END);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_FINGER) == EQUIPMENT_SLOT_END);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_TRINKET) == EQUIPMENT_SLOT_END);
  CHECK(EquipmentSlotForInventoryType(INVTYPE_BAG) == EQUIPMENT_SLOT_END);
}

TEST_CASE("transmog: slot acceptance handles hand and chest/robe equivalence")
{
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_HEAD, INVTYPE_HEAD));
  CHECK_FALSE(SlotAcceptsInventoryType(EQUIPMENT_SLOT_HEAD, INVTYPE_CHEST));

  // One-hand weapons fit either hand.
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_MAINHAND, INVTYPE_WEAPON));
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_OFFHAND, INVTYPE_WEAPON));
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_OFFHAND, INVTYPE_SHIELD));
  CHECK_FALSE(SlotAcceptsInventoryType(EQUIPMENT_SLOT_MAINHAND, INVTYPE_SHIELD));

  // Chest and robe share a slot.
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_CHEST, INVTYPE_ROBE));
  CHECK(SlotAcceptsInventoryType(EQUIPMENT_SLOT_CHEST, INVTYPE_CHEST));

  // A two-hander only occupies the main hand.
  CHECK_FALSE(SlotAcceptsInventoryType(EQUIPMENT_SLOT_OFFHAND, INVTYPE_2HWEAPON));
}

TEST_CASE("transmog: equipment slot names are stable")
{
  CHECK(std::string(EquipmentSlotName(EQUIPMENT_SLOT_HEAD)) == "Head");
  CHECK(std::string(EquipmentSlotName(EQUIPMENT_SLOT_OFFHAND)) == "Off Hand");
  CHECK(std::string(EquipmentSlotName(EQUIPMENT_SLOT_MAINHAND)) == "Main Hand");
}
