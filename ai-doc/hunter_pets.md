# Hunter pets

How the vendor's hunter pet options work and the core changes that make the
server hunter-friendly.

## Vendor options (hunter only)

`ItemVendor`'s gossip menu gains two entries for hunters
(`player->IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET)`), between "Meta Gems" and
the utility options:

- **Pet Stable** - uses the client-native stable option type
  (`GOSSIP_OPTION_STABLEPET`); `ItemVendor::CanCreatureGossipSelect` falls
  through to the core, whose `Player::OnGossipSelect` serves the stable window.
  This requires the vendor to carry `UNIT_NPC_FLAG_STABLEMASTER` (set in
  `OnCreatureAddWorld`), which `WorldSession::CheckStableMaster` checks.
- **Get Pet** - custom action `ItemVendor::UtilGetPet` (9000004), handled by
  `arenacraft::HandleHunterPetAction`
  (`src/game/Arenacraft/npcs/HunterPets.cpp`), which re-sends the gossip menu
  listing the pet families. Each family entry uses action
  `PetFamilyActionBase + i` (9001000 + i).

## Pet families

`PetFamilies` in `HunterPets.cpp` maps a family name to one tameable Northrend
representative creature (so the model fits a level-80 pet; each verified in
`creature_template`: type beast, tameable type flag, level ~70-80):

| Family | Entry | Representative          |
| ------ | ----- | ----------------------- |
| Wolf   | 29358 | Frostworg               |
| Cat    | 29327 | Frost Leopard           |
| Spider | 31747 | Necrotic Webspinner     |
| Bear   | 29319 | Icepaw Bear             |
| Boar   | 29996 | Snorts                  |
| Crab   | 26521 | Kili'ua                 |

Adding a family is one row there. Pick a tameable Northrend creature of that
family; the pet gets its name from its `CreatureFamilyEntry` (Wolf, Bear, ...),
not from the row.

## Grant flow (`arenacraft::GrantPetFamily`)

- Requires a free stable slot; all four `PetStable::StabledPets` slots are
  scanned regardless of `MaxStabledPets`. A full stable is rejected with a
  directed `PSendSysMessage` and nothing is granted.
- Builds the pet like the tame flow's `Unit::InitTamedPet`
  (`CreateBaseAtCreatureInfo` -> creator/faction/flags ->
  `InitStatsForLevel(80)` -> `CharmInfo::SetPetNumber(GeneratePetNumber())` ->
  `InitPetCreateSpells` (family passives + level-up spells) -> `SetFullHealth`),
  then pins happiness to max and calls `InitTalentForLevel` for the level-80
  talent points (a pet created straight at 80 otherwise ends up with none).
- `SavePetToDB(PET_SAVE_FIRST_STABLE_SLOT + slot)` writes it to `character_pet`;
  `FillPetInfo` mirrors it into `PetStable::StabledPets[slot]`. The pet object
  is then deleted and never summoned - it is a normal stabled pet from the
  player's point of view (take it out via the stable window).
- `SetOwnerGUID` (UNIT_FIELD_SUMMONEDBY) is set directly instead of via
  `Unit::SetMinion`, because SetMinion would also mark the player as having an
  active pet pointing at a non-world object.
- The pet keeps `UNIT_CAN_BE_RENAMED`, so the hunter can rename it.

## Perma-happy pets (core changes)

`src/game/Entities/Pet/Pet.cpp`:

- `Pet::LoseHappiness()` is a no-op - the periodic decay from `Pet::Update`
  (every 7.5s) does nothing.
- `Pet::setDeathState` no longer subtracts happiness when a hunter pet dies.
- `Pet::LoadPetFromDB` pins `POWER_HAPPINESS` to max on load, so pets saved
  before the change come out fully happy too.

Feeding still adds happiness; nothing subtracts it anymore, so pets are
permanently HAPPY - which also means the permanent +25% pet damage bonus
(`Pet::GetHappinessState`, used by `StatSystem.cpp`).

## Stable slots

- New hunters get all four slots at creation:
  `arenacraft::GrantStartingStableSlots` (see `ai-doc/character_creation.md`).
  `PetStable::MaxStabledPets` persists in `characters.stableSlots` and is
  restored by `Player::_LoadPetStable`.
- Hunters created before this change still carry whatever `stableSlots` says
  (usually 0): the first "Get Pet" grant sets `MaxStabledPets =
  MAX_PET_STABLES`, or they can buy slots through the stable UI
  (`HandleBuyStableSlot`) as normal.
