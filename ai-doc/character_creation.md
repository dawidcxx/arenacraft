# Character creation (arenacraft)

A new character is fully set up in `Player::Create`
(`src/game/Entities/Player/Player.cpp`) so nothing has to run on first login
(no `PlayerScript::OnFirstLogin` animations/scripts).

What a fresh character gets:

- **No gear.** The race/class start outfit (`CharStartOutfit`) and any
  `playercreateinfo_item` rows are deliberately not applied - characters start
  naked and gear comes from the vendors. No SQL change is needed for this; the
  application code was removed.
- **Bags.** A Frostweave Bag (`41599`) is equipped into each of the four carried
  bag slots (`INVENTORY_SLOT_BAG_START..END`). Bank bags are not filled (bank
  bag slots must be purchased first).
- **Weapon proficiencies.** `arenacraft::GrantStartingWeaponSkills`
  (`src/game/Arenacraft/CharacterCreation.cpp`) learns the class's weapon
  proficiency spells and then calls `Player::UpdateSkillsToMaxSkillsForLevel()`
  to cap the matching skills. The class -> proficiency table mirrors the known
  class weapon lists, so e.g. a mage can use daggers but not axes.
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

The helpers are called from `Player::Create` right after
`LearnDefaultSkills()` / `LearnCustomSpells()`, except `GrantStartingTotems`,
which runs after the starting bags are equipped so the items have somewhere to
go. Extend the tables in `CharacterCreation.cpp` to change what classes get.
