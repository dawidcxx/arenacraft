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
| 29659 | Positive Charge (arena dampening, "Dementia buff") | forced-debuff flag cleared (rendered as a buff); stack cap 99 -> 2; never saved (`SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED`) | `ApplySpellFix({29659}, [](SpellInfo* spellInfo) { spellInfo->Attributes &= ~SPELL_ATTR0_AURA_IS_DEBUFF; spellInfo->StackAmount = 2; spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED; });` | Arena dampening ("Dementia buff"): from 5 min after gates open, +1 stack at 5 and 7 min, cap 2 (+20% damage), applied to players and their pets/guardians. See `ai-doc/dampening.md` | 2026-09-23 |
| 51730, 51988, 51991, 51992, 51993, 51994 | Earthliving Weapon ranks 1-6 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({51730, 51988, 51991, 51992, 51993, 51994}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Earthliving Weapon is a rogue-poison-style class imbue and must persist through arena entry | 2026-10-11 |
| 8033, 8038, 10456, 16355, 16356, 25500, 58794, 58795, 58796 | Frostbrand Weapon ranks 1-9 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({8033, 8038, 10456, 16355, 16356, 25500, 58794, 58795, 58796}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Same as Earthliving: shaman weapon imbue must persist through arena entry | 2026-10-11 |
| 8232, 8235, 10486, 16362, 25505, 58801, 58803, 58804 | Windfury Weapon ranks 1-8 (Shaman) | `AttributesEx2` gains `SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY` (0x2000) | `ApplySpellFix({8232, 8235, 10486, 16362, 25505, 58801, 58803, 58804}, [](SpellInfo* spellInfo) { spellInfo->AttributesEx2 |= SPELL_ATTR2_ENCHANT_OWN_ITEM_ONLY; });` | Same as Earthliving: shaman weapon imbue must persist through arena entry | 2026-10-11 |

## Mechanic divergences

Deliberate core behavior changes that are not DBC-value tweaks. Listed here so
they can be found and reverted.

| Area | Change | Code location | Reason | Date |
|------|--------|---------------|--------|------|
| Diminishing CC | Re-casting your own CC of a diminishing group on a player/pet is always allowed, but it only lands when it would hold longer than your current aura of that group on the target: the duration it would get (talents, glyphs, the next diminishing level, the target's duration reductions) is compared with what is left of the current one. A weaker one bounces with `SPELL_FAILED_AURA_BOUNCED` ("a more powerful spell is already active") and does not advance diminishing; only the aura bounces, damage and other effects still land. Matching is by diminishing group **and** caster GUID, so another player's CC of the same group still lands | `HasOwnStrongerDiminishedAura`, `Spell::DoSpellHitOnUnit` (`src/game/Spells/Spell.cpp`) | Recasting your own CC (e.g. Polymorph) must not replace the current aura with a shorter, diminished one; but it must be castable (#36: re-Sheep / re-Fear could not even be started while the CC had more than half of its duration left) | 2026-11-04 |
| Totem placement | Shaman totems (summon type totem on the four caster-corner targets) always get separate spots: the slot offset (3 yd) if reachable, else another reachable direction around the shaman at full or half distance, never within 1 yd of the shaman or of the shaman's other totems; last resort is a 0.75 yd step towards the slot. An airborne shaman searches from the ground/water level below | `SelectTotemSlotPosition`, `Spell::SelectImplicitCasterDestTargets` (`src/game/Spells/Spell.cpp`) | A failed collision check (shaman in the air >7 yd above the navmesh, or standing where there is no navmesh) left every totem at the shaman's own position, stacked in one point | 2026-10-13 |
| Shared proc rolls | Within one proc event (a spell landing on one target, or one melee/ranged swing) every proc of the same unit with the same chance uses one roll, so they happen together or not at all (talents, trinkets, `SPELL_AURA_ADD_TARGET_TRIGGER` hit triggers). Frostbite 3/3 and Fingers of Frost 2/2 (both 15%) therefore always proc together; the Frostbite/FoF-specific synchronisation is gone. Fingers of Frost only procs when the chill lands (hit phase). Different chances, units or events stay independent | `src/game/Arenacraft/ProcRolls.cpp`, `Spell::DoAllEffectOnTarget`/`DoTriggersOnSpellHit` (`Spell.cpp`), `Unit::ProcDamageAndSpell`/`IsTriggeredAtSpellProcEvent` (`Unit.cpp`), `Aura::IsProcTriggeredOnEvent` (`SpellAuras.cpp`) | Procs with the same chance co-occur on retail (Frostbite + Fingers of Frost, same-chance trinkets); fixing only FoF treated the symptom (#35) | 2026-11-04 |
| Weapon poison duration | Rogue family temporary weapon enchants (poisons) last 7 days instead of the DBC 30 min / 1 h | `Spell::EffectEnchantItemTmp` (`src/game/Spells/SpellEffects.cpp`) | No poison reapplication while gearing up | 2026-09-19 |
| Gargoyle damage | Ebon Gargoyle spell power = 33.3% of the DK's attack power snapshot at summon, +4% per Impurity rank (40% at 5/5; was a flat 33%, or 40% with any Impurity rank). Gargoyle Strike (51963) direct coefficient overridden 0.453 -> 1.0, so a strike scales with 0.4 AP at 5/5 Impurity instead of ~0.18 | `spell_dk_pet_scaling::CalculateSPAmount` (`src/game/Scripts/Spells/spell_dk.cpp`), `ApplySpellBonusCorrections` (`src/game/Spells/SpellMgr.cpp`) | The DB coefficient on top of the 33% AP->SP conversion halved the gargoyle's damage vs retail 3.3.5 (0.4 AP per strike, 3.1-era testing) | 2026-10-13 |
| Hunter pet basic attacks | Claw / Bite / Smack (all ranks) attack-power coefficient 0.07 -> 0.364 of the pet's AP (pet AP = 22% of the hunter's RAP, so ~8% of RAP per hit instead of ~1.5%); the pet spell-power share (0.119658) is kept, ~9.5% RAP in total | `ApplySpellBonusCorrections` (`src/game/Spells/SpellMgr.cpp`) | BM pets dealt too little damage (#20); upper end of the community formula base + RAP x 0.40 x 0.20 (TrinityCore #3253) | 2026-11-03 |
| Tamed pet happiness | A pet tamed with Tame Beast starts fully happy (was 166500 = unhappy, 75% damage, until re-summoned); happiness never decays (`Pet::LoseHappiness`) | `Pet::CreateBaseAtTamed` (`src/game/Entities/Pet/Pet.cpp`) | Pets are permanently happy on this server | 2026-11-03 |
| Omen of Clarity on spells | Clearcasting (16864) proc chance on spells = 4.5 PPM measured on the spell's cast time (at least 1.5s) instead of the druid's weapon speed, then a fail chance: cast-time spells 33%, instants/channels 33% on Blizzard's 3.5 PPM instead of 4.5 (= 5.86%; Blizzard: 92.4%), Wild Growth/Typhoon 75%, Hurricane ticks 7/9; triggered spells (Starfall stars, ...) never proc it. Auto attacks and on-next-swing attacks (Maul) keep the weapon-speed 3.5 PPM. Shred, Mangle (cat/bear) and Feral Charge (cat/bear) also proc on top of that at the same weapon-speed rate: 5% per hit in cat form (1.0s attack time), 14% in bear form (2.5s); other melee-damage-class abilities are still filtered out by the aura script (`spell_dru_omen_of_clarity::CheckProc`). E.g. a 1.5s cast 7.5%, Healing Touch (2.5s) 12.5%, an instant 5.86% (0.86% with the 92.4% fail; was ~19-21% on everything with a staff) | `OmenOfClaritySpellChance`, `procsAtWeaponSpeed`, `Unit::IsTriggeredAtSpellProcEvent` (`src/game/Entities/Unit/Unit.cpp`), `spell_dru_omen_of_clarity::CheckProc` (`src/game/Scripts/Spells/spell_druid.cpp`), `src/game/Arenacraft/OmenOfClarity.cpp` | Clearcasting procced far too often (azerothcore-wotlk#24395); tuned up from Blizzard's 3.5 PPM because the original fix felt weak, and Shred/Mangle/Feral Charge added so feral abilities can proc too | 2026-11-07 |
| Weapon imbue duration | Shaman family temporary weapon enchants (Earthliving/Frostbrand/Windfury/Flametongue) last 7 days instead of the DBC 30 min | `Spell::EffectEnchantItemTmp` (`src/game/Spells/SpellEffects.cpp`) | Same as poisons: no imbue reapplication while gearing up | 2026-10-11 |
| Arena/BG dampening (74410/74411) | Added `EFFECT_1` = `SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN` (`BasePoints -6` = -5% damage taken, all schools). `EFFECT_0` remains the DBC `SPELL_AURA_MOD_HEALING_DONE_PERCENT` -10%, so the healing reduction is not replaced | `ApplySpellFix({74410, 74411}, [](SpellInfo* spellInfo) { spellInfo->Effects[EFFECT_1].Effect = SPELL_EFFECT_APPLY_AURA; spellInfo->Effects[EFFECT_1].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN; spellInfo->Effects[EFFECT_1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER); spellInfo->Effects[EFFECT_1].DieSides = 1; spellInfo->Effects[EFFECT_1].BasePoints = -6; spellInfo->Effects[EFFECT_1].MiscValue = SPELL_SCHOOL_MASK_ALL; });` (`src/game/Spells/SpellInfoCorrections.cpp:4644`) | Arena/BG games are too bursty; applies wherever `spell_area` already auto-casts 74410/74411 | 2026-11-05 |
| Pet/guardian damage taken | Pets and guardians whose player owner has 74410 or 74411 take 10% less damage (multiplicative with resilience/other damage-taken mods). Their own damage output is unchanged, but player targets also take 5% less from them | `IsArenacraftDampeningPet` (`src/game/Entities/Unit/Unit.cpp:118`), `ARENACRAFT_PET_DAMPENING_DAMAGE_TAKEN` (`:116`), applied in `Unit::SpellDamageBonusTaken` (`:12137`) and `Unit::MeleeDamageBonusTaken` (`:13676`) | Pets died too fast in arenas/BGs; makes them tankier without reducing their damage output | 2026-11-05 |
| Rolling DoTs (Deep Wounds, Ignite, ...) | No DoT munching: the remaining damage of the existing aura is rolled into the new application and cast immediately. The 400 ms batch queue is gone (it queued every crit with a value computed before the batch ran, so each application overwrote the previous one instead of adding to it; only one per 400 ms window survived). Also no longer passes an unrelated periodic aura as `triggeredByAura` when no matching aura is found | `Unit::CastRolledPeriodicSpell` (`src/game/Entities/Unit/Unit.cpp:20811`), removed `AuraMunchingQueue` (`src/game/Entities/Unit/Unit.cpp`) and `CONFIG_MUNCHING_BLIZZLIKE` (`src/game/World/IWorld.h`, `src/game/World/World.cpp`) | Deep Wounds stacked far too low vs 3.3.5 (single application was correct; repeated crits inside one batch were lost). Same over-munching affected Ignite/Electrified/T10 procs | 2026-11-11 |

### Notes

- The full two-layer mechanic — static `74410`/`74411` plus the ramping arena
  `29659` that players call the **"Dementia buff"** — is documented in
  `ai-doc/dampening.md`.
- Percent values encode as `BasePoints + 1`: `-91` = 90%, `-101` = 100%,
  `9` = 10%, `14` = 15%, `109` = 110%, `129` = 130%.
- The new -5% damage taken on `74410`/`74411` stacks multiplicatively with
  arena dampening `29659` (`AddPct`): at 29659's 2-stack cap the net is
  `1.20 * 0.95 = +14%`, not +15%.
- The pet 10% damage-taken reduction is keyed off the owner having `74410` or
  `74411`, so its scope matches the existing area aura (including Wintergrasp
  during war time, where `74411` is applied). It covers pets and guardians but
  not totems or vehicles.
- `29659` is the permanent, undispellable `SPELL_AURA_MOD_DAMAGE_PERCENT_DONE`
  aura reused as arena dampening. Its DBC `AURA_IS_DEBUFF` flag is what marked
  it harmful; clearing it makes the aura land in the buff row. Its `BasePoints`
  is locked by the shared `BasePoints + 1` rule (`9` = 10%/stack). The
  gates-open timer and stacking live in `Arena::PreUpdateImpl`
  (`src/game/Battlegrounds/Arena.cpp`), not in the fix. It is applied to every
  player and every alive unit in their `m_Controlled` (pets, guardians), and it
  is marked unsaveable so a hunter/warlock pet cannot carry it out of the arena.
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
