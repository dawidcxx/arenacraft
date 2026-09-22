# Character creation (arenacraft)

A new character is set up in `Player::Create`
(`src/game/Entities/Player/Player.cpp`) so almost nothing has to run on first
login (no `PlayerScript::OnFirstLogin` animations/scripts). The one exception is
reputations (see below), which are still seeded in the `AT_LOGIN_FIRST` branch of
`WorldSession::HandlePlayerLogin`.

What a fresh character gets:

- **No gear.** The race/class start outfit (`CharStartOutfit`) and any
  `playercreateinfo_item` rows are deliberately not applied - characters start
  naked and gear comes from the vendors. No SQL change is needed for this; the
  application code was removed.
- **Bags.** A Frostweave Bag (`41599`) is equipped into each of the four carried
  bag slots (`INVENTORY_SLOT_BAG_START..END`). Bank bags are not filled (bank
  bag slots must be purchased first).
- **Hearthstone + homebind.** Because the start outfit is skipped, the
  Hearthstone (`6948`) is added explicitly in `Player::Create`. The character's
  default homebind - set in `Player::_LoadHomeBind` when no `character_homebind`
  row exists - is the arena hub in Netherstorm (map 530,
  `3369.4014 2882.7666 143.8963`), but it is reported as area `3877`
  (Eco-Dome Midrealm) so the Hearthstone tooltip stays generic. Binding at an
  innkeeper still overrides it, and the Hearthstone is also stocked first on the
  general goods vendor.
- **Weapon proficiencies.** `arenacraft::GrantStartingWeaponSkills`
  (`src/game/Arenacraft/CharacterCreation.cpp`) learns the class's weapon
  proficiency spells and then calls `Player::UpdateSkillsToMaxSkillsForLevel()`
  to cap the matching skills. The class -> proficiency table mirrors the known
  class weapon lists, so e.g. a mage can use daggers but not axes.
- **Armor proficiencies.** `arenacraft::GrantStartingArmorProficiencies` learns
  the top-tier armor proficiency that normally requires a trainer: plate (750)
  for warrior/paladin/death knight and mail (8737) for hunter/shaman. The lower
  tiers (cloth/leather/mail) already come from `playercreateinfo_skills`, so
  everyone can equip their vendor gear straight away. No-op for the other
  classes.
- **Class ability set.** `arenacraft::GrantStartingSpells`
  (`src/game/Arenacraft/StartingSpells.cpp`) learns the class's complete level-80
  kit (every rank) so an instant-80 character never visits a trainer. Talents
  are deliberately excluded from `ClassSpells` - the player is meant to pick
  them up with talent points. Note that a talent's real ability is often a
  *triggered* spell, not the rank spell itself (e.g. Impact, Mangle
  (Cat/Bear), Shamanistic Rage), so `GetTalentSpellPos` alone is not enough to
  catch one. The custom class trainer rows (`npc_trainer`, ids `2000xx`) are the
  authority for what a class may otherwise train for free.
- **Mount + riding.** `arenacraft::GrantStartingMount` learns the four Riding
  ranks (33388/33389/34090/34091), Cold Weather Flying (54197) and the Magic
  Rooster mount (65917) as spells, so nothing lands in the bags.
- **First Aid.** `arenacraft::GrantStartingFirstAid` raises the First Aid
  secondary skill (`SKILL_FIRST_AID`, 129) to the level cap (450) via
  `Player::SetSkill`, which also learns the rewarded bandage ranks.
- **Shaman totems.** `arenacraft::GrantStartingTotems` puts the four starting
  totems - Earth (5175), Fire (5176), Water (5177), Air (5178) - into the bags
  (the totem quests are skipped on a level-80-only server). No-op for other
  classes.
- **Pet stable slots.** `arenacraft::GrantStartingStableSlots` unlocks all four
  hunter stable slots (`PetStable::MaxStabledPets`, persisted in
  `characters.stableSlots`). No-op for other classes. See
  `ai-doc/hunter_pets.md`.
- **Reputations.** On first login (`AT_LOGIN_FIRST`, config
  `PlayerStart.AllReputation`) a curated set of WotLK factions - Cenarion
  Expedition, the Northrend quarters (Kirin Tor, Wyrmrest Accord, Ebon Blade,
  Argent Crusade, Sons of Hodir, Kalu'ak, Frenzyheart/Oracles, Ashen Verdict)
  and the character's own capital cities + expedition subfactions - is set to
  Exalted (42999). The standings are then sent as a single
  `SMSG_INITIALIZE_FACTIONS` re-send (`ReputationMgr::SendInitialReputations`)
  rather than a burst of `SMSG_SET_FACTION_STANDING` packets, and persisted by
  the normal `SaveToDB` so relogs need no special handling. The curated ID lists
  live in `WorldSession::HandlePlayerLogin`
  (`src/game/Handlers/CharacterHandler.cpp`).

The helpers are called from `Player::Create` right after
`LearnDefaultSkills()` / `LearnCustomSpells()`, except `GrantStartingTotems`,
which runs after the starting bags are equipped so the items have somewhere to
go. Extend the tables in `CharacterCreation.cpp` (proficiencies, mount, totems)
or the `ClassSpells` table in `StartingSpells.cpp` (learned abilities) to change
what classes get.
