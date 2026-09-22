# Transmogrification

Appearance-only item transmog, driven from the cosmetics NPC (entry 19728, see
`ai-doc/vendor.md`). Plain core code, no config, no module layer:

- `src/game/Arenacraft/transmog/Transmogrification.hpp/.cpp` - the system.
- `src/game/Arenacraft/npcs/CosmeticVendor.cpp` - the gossip menus that call it.
- `src/game/Arenacraft/transmog/Transmogrification_test.cpp` - tests for the
  pure helpers.

## How it renders

A transmog never touches the real item. It stores `item GUID -> fake item id`
and, whenever the core fills the player's visible equipment
(`Player::SetVisibleItemSlot`), the `TransmogPlayerScript` hook
(`OnAfterSetVisibleItemSlot`) overwrites `PLAYER_VISIBLE_ITEM_1_ENTRYID + slot*2`
with the fake item's entry and calls `ForceValuesUpdateAtIndex`. That field is
purely cosmetic, is broadcast to everyone, and is what the client renders the
model from, so:

- stats, enchants and **set bonuses are untouched** (the real item is unchanged)
- everyone sees the look, including the player themselves
- no fake items are created, so there is nothing to leak or desync

The fake entry is always validated against `sObjectMgr->GetItemTemplate` and
must have a non-zero `DisplayInfoID` before it is written, so the client can
always resolve it.

## Data / persistence

`character_transmogrification (ItemGuid, OwnerGuid, FakeEntry)` in the characters
DB (migration `data/sql/updates/db_characters/2026_09_22_01.sql`). Items are
keyed by their global GUID, which is never reused. `OwnerGuid` is checked on
apply so a transmogged item that is traded/mailed does not carry its appearance
to the next owner (the new owner's login query filters by `OwnerGuid` anyway).

The in-memory cache (`item GUID -> {owner, fake}`) is loaded on login, dropped
on logout, and guarded by a mutex because visible-slot updates run on map
threads while login/logout run on the world thread. Login loads after the
character's items, and then re-applies the looks to the visible slots.

Orphan rows (item destroyed) are ignored by the `item_instance` join on load and
harmless; they are not currently pruned.

## Rules

- only **rare, epic and legendary** items may be used, as appearance AND target
  (the "no trashmogs" rule); epic into legendary is allowed
- source and target must be the same item class (armor/weapon) and the same
  subclass, i.e. **armor types must match** (cloth/leather/mail/plate; weapons
  need the same weapon type)
- the source's inventory type must fit the target's slot; chest/robe share a
  slot and a plain one-hand weapon fits either hand (main hand is preferred when
  both hands hold a compatible weapon)
- rings, trinkets, necks, bags, ammo, relics etc. cannot be transmogrified (no
  inventory type -> slot mapping)
- set bonuses are never invalidated (see above)

## Menus (CosmeticVendor)

- main menu: **Exotic mounts** (vendor) + **Transmog by item ID** + **Clear
  Transmog**
- **Transmog by item ID** is a coded gossip option, so clicking it drops the
  player straight into the client's text box (no intermediate menu); the id is
  parsed and applied on submit
- **Clear Transmog** lists one option per equipped slot that currently has a
  transmog (`EquipmentSlotName`, e.g. "Off Hand") plus Back; picking one clears
  it and refreshes the list

Actions are the `CosmeticVendor::Action*` constants; the clear entries are
`ActionClearSlotBase + slot`.
