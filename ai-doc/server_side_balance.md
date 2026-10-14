# Server-side balance adjustments

Living log of intentional gameplay-balance divergences from the 3.3.5 DBC data.
These are **server-side only**: we deliberately do NOT ship a matching client DBC
edit, so the in-game tooltip may show the old value. We accept that mismatch.

## Rule

- Balance tweaks go directly in the core as static C++, never in SQL/DB content
  (see `AGENTS.md`).
- Spell-value changes are bare `ApplySpellFix(...)` one-liners appended at the end
  of `SpellMgr::LoadSpellInfoCorrections()`
  (`src/game/Spells/SpellInfoCorrections.cpp`), right before its closing
  `LOG_INFO`. Keep value tweaks comment-free - this doc is their registry.
  Non-value behavior fixes (flags/mechanics) may carry a short explanatory
  comment in the C++ as well.
- Every change MUST be listed in the table below (with its exact code line) so it
  can be found and reverted later. These are tuning knobs, not permanent fixes.
- Do NOT touch client DBC/tooltips for balance reasons.
- Removing a change = delete its `ApplySpellFix` line + its row here. Original
  behavior is the untouched DBC value.

## Entries

| Spell ID | Name | Change | Code line | Reason | Date |
|----------|------|--------|-----------|--------|------|
| 31224 | Cloak of Shadows (Rogue) | spell avoidance 90% -> 100% | `ApplySpellFix({31224}, [](SpellInfo* spellInfo) { spellInfo->Effects[EFFECT_0].BasePoints = -101; });` | Full spell avoidance during the window | 2026-09-21 |
| 35449 | Improved Mortal Strike rank 3 (Warrior) | Mortal Strike dmg bonus 10% -> 15% | `ApplySpellFix({35449}, [](SpellInfo* spellInfo) { spellInfo->Effects[EFFECT_0].BasePoints = 14; });` | Compensate warriors for imperfect charge pathing | 2026-09-21 |
| 53385 | Divine Storm (Paladin) | weapon damage 110% -> 130% | `ApplySpellFix({53385}, [](SpellInfo* spellInfo) { spellInfo->Effects[EFFECT_2].BasePoints = 129; });` | Buff Divine Storm damage | 2026-09-21 |
| 29659 | Positive Charge (arena dampening) | forced-debuff flag cleared (rendered as a buff); stack cap 99 -> 2 | `ApplySpellFix({29659}, [](SpellInfo* spellInfo) { spellInfo->Attributes &= ~SPELL_ATTR0_AURA_IS_DEBUFF; spellInfo->StackAmount = 2; });` | Arena dampening: from 5 min after gates open, +1 stack at 5 and 7 min, cap 2 (+20% damage) | 2026-09-23 |
| 51730, 51988, 51991, 51992, 51993, 51994 | Earthliving Weapon ranks 1-6 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({51730, 51988, 51991, 51992, 51993, 51994}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Earthliving Weapon is a rogue-poison-style class imbue and must persist through arena entry | 2026-10-11 |
| 8033, 8038, 10456, 16355, 16356, 25500, 58794, 58795, 58796 | Frostbrand Weapon ranks 1-9 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({8033, 8038, 10456, 16355, 16356, 25500, 58794, 58795, 58796}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Same as Earthliving: shaman weapon imbue must persist through arena entry | 2026-10-11 |
| 8232, 8235, 10486, 16362, 25505, 58801, 58803, 58804 | Windfury Weapon ranks 1-8 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({8232, 8235, 10486, 16362, 25505, 58801, 58803, 58804}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Same as Earthliving: shaman weapon imbue must persist through arena entry | 2026-10-11 |

## Mechanic divergences

Deliberate core behavior changes that are not DBC-value tweaks. Listed here so
they can be found and reverted.

| Area | Change | Code location | Reason | Date |
|------|--------|---------------|--------|------|
| Diminishing CC | Same-applicator CC cannot be re-applied over a player/pet target while the existing aura of the same diminishing group still has more than 50% of its duration left; the hit is rejected at application time (cast bar finishes) with `SPELL_FAILED_AURA_BOUNCED` and does not advance diminishing. Matching is by diminishing group **and** caster GUID, so a different player's CC of the same group still lands | `Spell::DoSpellHitOnUnit` (`src/game/Spells/Spell.cpp`) | Recasting your own CC (e.g. Polymorph) must not replace the current aura with a shorter, diminishing one when the victim did not trinket; only time or a CC break may shorten/remove existing CC | 2026-09-25 |
| Totem placement | Shaman totems (summon type totem on the four caster-corner targets) always get separate spots: the slot offset (3 yd) if reachable, else another reachable direction around the shaman at full or half distance, never within 1 yd of the shaman or of the shaman's other totems; last resort is a 0.75 yd step towards the slot. An airborne shaman searches from the ground/water level below | `SelectTotemSlotPosition`, `Spell::SelectImplicitCasterDestTargets` (`src/game/Spells/Spell.cpp`) | A failed collision check (shaman in the air >7 yd above the navmesh, or standing where there is no navmesh) left every totem at the shaman's own position, stacked in one point | 2026-10-13 |
| Weapon poison duration | Rogue family temporary weapon enchants (poisons) last 7 days instead of the DBC 30 min / 1 h | `Spell::EffectEnchantItemTmp` (`src/game/Spells/SpellEffects.cpp`) | No poison reapplication while gearing up | 2026-09-19 |
| Gargoyle damage | Ebon Gargoyle spell power = 33.3% of the DK's attack power snapshot at summon, +4% per Impurity rank (40% at 5/5; was a flat 33%, or 40% with any Impurity rank). Gargoyle Strike (51963) direct coefficient overridden 0.453 -> 1.0, so a strike scales with 0.4 AP at 5/5 Impurity instead of ~0.18 | `spell_dk_pet_scaling::CalculateSPAmount` (`src/game/Scripts/Spells/spell_dk.cpp`), `ApplySpellBonusCorrections` (`src/game/Spells/SpellMgr.cpp`) | The DB coefficient on top of the 33% AP->SP conversion halved the gargoyle's damage vs retail 3.3.5 (0.4 AP per strike, 3.1-era testing) | 2026-10-13 |
| Weapon imbue duration | Shaman family temporary weapon enchants (Earthliving/Frostbrand/Windfury/Flametongue) last 7 days instead of the DBC 30 min | `Spell::EffectEnchantItemTmp` (`src/game/Spells/SpellEffects.cpp`) | Same as poisons: no imbue reapplication while gearing up | 2026-10-11 |

### Notes

- Percent values encode as `BasePoints + 1`: `-91` = 90%, `-101` = 100%,
  `9` = 10%, `14` = 15%, `109` = 110%, `129` = 130%.
- `29659` is the permanent, undispellable `SPELL_AURA_MOD_DAMAGE_PERCENT_DONE`
  aura reused as arena dampening. Its DBC `AURA_IS_DEBUFF` flag is what marked
  it harmful; clearing it makes the aura land in the buff row. Its `BasePoints`
  is locked by the shared `BasePoints + 1` rule (`9` = 10%/stack). The
  gates-open timer and stacking live in `Arena::PreUpdateImpl`
  (`src/game/Battlegrounds/Arena.cpp`), not in the fix.
- `31224` also exists as unpublished duplicates `39666`/`65961`; only `31224` is
  granted to rogues (`src/game/Arenacraft/StartingSpells.cpp`).
- `53385`: `EFFECT_2` is `SPELL_EFFECT_WEAPON_PERCENT_DAMAGE`; `EFFECT_1` (25%
  heal) and the unused `EFFECT_0` dummy are left alone.
- The shaman imbue fixes all add the same `AttributesEx2` bit `0x2000`
  (`SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY`, commented "Enchant persists when entering
  arena"). It is the flag `SpellMgr::LoadEnchantCustomAttr` uses to register the
  spell's temporary enchant as arena-legal, so
  `Player::RemoveArenaEnchantments` (`Battleground::AddPlayer` on arena entry)
  keeps it - exactly what poisons and DK runes already rely on. The DBC ships the
  flag unset on every shaman imbue.
- Enabled so far: Earthliving, Frostbrand and Windfury. Flametongue is still
  unset and thus stripped on arena entry. Rockbiter is intentionally left out:
  its granted ranks carry a `MiscValue` of 0 and the real enchant is picked at
  cast time by `Spell::EffectEnchantItemTmp` (`SpellEffects.cpp`) from the
  weapon's dps, so enabling it would mean flagging a pile of trigger spells.
