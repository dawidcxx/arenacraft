# Server-side balance adjustments

Living log of intentional gameplay-balance divergences from the 3.3.5 DBC data.
These are **server-side only**: we deliberately do NOT ship a matching client DBC
edit, so the in-game tooltip may show the old value. We accept that mismatch.

## Rule

- Balance tweaks go directly in the core as static C++, never in SQL/DB content
  (see `AGENTS.md`).
- Spell-value changes live in the clearly marked **Arenacraft server-side balance
  adjustments** block at the end of `SpellMgr::LoadSpellInfoCorrections()`
  (`src/game/Spells/SpellInfoCorrections.cpp`).
- Every change MUST be listed in the table below so it can be found and reverted
  later (these are tuning knobs, not permanent fixes).
- Do NOT touch client DBC/tooltips for balance reasons.
- Removing a change = delete its `ApplySpellFix` line in the block + its row
  here. The original behavior is the untouched DBC value.

## Entries

| Spell ID | Name | Class | Field | Old | New | Reason | Date |
|----------|------|-------|-------|-----|-----|--------|------|
| 31224 | Cloak of Shadows | Rogue | `Effects[EFFECT_0].BasePoints` (aura 186, attacker spell hit chance) | `-91` (−90% hit) | `-101` (−100% hit) | Full spell avoidance during the window as a balance change | 2026-09-21 |
| 35449 | Improved Mortal Strike (rank 3) | Warrior | `Effects[EFFECT_0].BasePoints` (aura 108, `ADD_PCT_MODIFIER` on Mortal Strike) | `9` (+10% dmg) | `19` (+20% dmg) | Compensate warriors for imperfect charge pathing | 2026-09-21 |

Notes on value encoding: percent auras are stored as `BasePoints + 1`, so
`-91` = 90% and `-101` = 100%. The same spell also exists as 39666 and 65961,
but rogues are granted 31224 (`src/game/Arenacraft/StartingSpells.cpp`), so only
that one is changed.
