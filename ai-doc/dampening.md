# Dampening ("Dementia buff")

How arena/battleground pressure is tuned on Arenacraft. Players usually call the
ramping arena buff the **"Dementia buff"**; internally it is the arena dampening
aura (spell `29659`, "Positive Charge").

There are two separate layers: a static area aura that applies in every arena
and battleground, and an arena-only buff that ramps up over the match.

## 1. Static arena/BG dampening (`74410` / `74411`)

Applied by base world DB `spell_area` rows:

- `74410` "Arena - Dampening" — arena subzones
  (`data/sql/base/db_world/spell_area.sql:659`)
- `74411` "Battleground - Dampening" — battleground zones/subzones
  (`data/sql/base/db_world/spell_area.sql:665`), including Wintergrasp only
  while war is running (`SpellArea::IsFitToRequirements`,
  `src/game/Spells/SpellMgr.cpp:1162`)

The core auto-casts these on area entry and removes them when the spell's
location check fails (`Player::UpdateZoneDependentAuras` /
`Player::UpdateAreaDependentAuras`, `src/game/Entities/Player/PlayerUpdates.cpp`).
They are hidden area auras: no client tooltip, no manual cast.

Effects:

| Effect | Spell | Value | Source |
| ------ | ----- | ----- | ------ |
| Healing done | `74410`, `74411` | **-10%** | DBC (`SPELL_AURA_MOD_HEALING_DONE_PERCENT`) |
| Damage done | `74410`, `74411` | **-5%** | `ApplySpellFix` (`SPELL_AURA_MOD_DAMAGE_PERCENT_DONE`) |

The -5% damage effect is added server-side in
`src/game/Spells/SpellInfoCorrections.cpp:4644` as `EFFECT_1`; the DBC's
`EFFECT_0` healing reduction is left untouched. Nothing in SQL changes: the
same `spell_area` rows now carry both effects.

Scope is exactly wherever `74410`/`74411` are auto-applied, i.e. every arena map
and every battleground zone/subzone (Wintergrasp only during war).

### Pets and guardians

Pets/guardians do **not** receive `74410`/`74411`. Instead, while their player
owner has either aura:

- pets and guardians take **10% less damage**
  (`IsArenacraftDampeningPet`, `src/game/Entities/Unit/Unit.cpp:118`), applied in
  `Unit::SpellDamageBonusTaken` (`:12137`) and `Unit::MeleeDamageBonusTaken`
  (`:13676`) using `ARENACRAFT_PET_DAMPENING_DAMAGE_TAKEN` (`:116`);
- their damage output is unchanged (they do not get the -5% damage-done effect).

Totems and vehicles are excluded; only `IsPet()` / `IsGuardian()` units qualify.

## 2. Ramping arena dampening — the "Dementia buff" (`29659`)

Arena only (not battlegrounds). Five minutes after the gates open, every player
and every alive unit in their `m_Controlled` (pets, guardians) gains one stack of
repurposed "Positive Charge" (`29659`); another stack follows every two minutes,
up to **2 stacks**.

- Each stack is **+10% damage done** (`SPELL_AURA_MOD_DAMAGE_PERCENT_DONE`), so
  the cap is **+20%**.
- The timer starts only once the match is `STATUS_IN_PROGRESS`, so the starting
  countdown and ready-crystal early starts do not count.
- The buff is permanent and undispellable for the match; it is not saved (a
  hunter/warlock pet cannot carry it out) and it is stripped on
  `Arena::RemovePlayerAtLeave`.
- DBC correction (`src/game/Spells/SpellInfoCorrections.cpp:4633`): clears
  `SPELL_ATTR0_AURA_IS_DEBUFF` (so it renders in the buff row), caps
  `StackAmount` at 2, and sets `SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED`.

Timer/stacking code lives in `Arena::PreUpdateImpl`
(`src/game/Battlegrounds/Arena.cpp:257`) with the tuning constants at
`src/game/Battlegrounds/Arena.cpp:122`:

| Constant | Value |
| -------- | ----- |
| `ARENA_DAMPENING_SPELL` | `29659` |
| `ARENA_DAMPENING_START_MS` | 5 minutes after gates open |
| `ARENA_DAMPENING_STEP_MS` | 2 minutes per stack |
| `ARENA_DAMPENING_MAX` | 2 stacks |

## Interaction between the layers

Percent done/taken modifiers use `AddPct`, which applies them
multiplicatively, so the two damage-done effects do not simply add:

| State | Damage done multiplier | Net |
| ----- | ---------------------- | --- |
| BG, no `29659` | `0.95` | **-5%** |
| Arena, 1 Dementia stack | `1.10 * 0.95` | **+4.5%** |
| Arena, 2 Dementia stacks | `1.20 * 0.95` | **+14%** |

Healing is **-10%** in both arenas and battlegrounds at all times. Pets take
**-10% damage** whenever their owner has the static area aura, on top of their
own resilience/defensive cooldowns.

## Why

Arena games were too bursty and healers were being pushed out. The static -5%
damage done reduces burst from the start, while the pet -10% damage taken keeps
pets from being deleted before they can contribute. The ramping `29659` remains
the end-of-match pressure valve.

## Tuning knobs

| What | Where |
| ---- | ----- |
| Healing reduction | DBC `74410`/`74411` `EFFECT_0` (do not edit client DBC) |
| Static damage reduction | `ApplySpellFix({74410, 74411}, ...)`, `SpellInfoCorrections.cpp:4644` |
| Pet damage-taken reduction | `ARENACRAFT_PET_DAMPENING_DAMAGE_TAKEN`, `Unit.cpp:116` |
| Dementia start/step/cap | `ARENA_DAMPENING_*`, `Arena.cpp:122` |
| Dementia per-stack value | `29659` DBC `BasePoints` / its `ApplySpellFix` |
| Area coverage | `spell_area` rows for `74410`/`74411`; `74411` war-time check in `SpellMgr.cpp:1162` |
