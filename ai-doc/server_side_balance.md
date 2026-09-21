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
  `LOG_INFO`. No comments in the C++ - this doc is the only registry.
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

### Notes

- Percent values encode as `BasePoints + 1`: `-91` = 90%, `-101` = 100%,
  `9` = 10%, `14` = 15%, `109` = 110%, `129` = 130%.
- `31224` also exists as unpublished duplicates `39666`/`65961`; only `31224` is
  granted to rogues (`src/game/Arenacraft/StartingSpells.cpp`).
- `53385`: `EFFECT_2` is `SPELL_EFFECT_WEAPON_PERCENT_DAMAGE`; `EFFECT_1` (25%
  heal) and the unused `EFFECT_0` dummy are left alone.
