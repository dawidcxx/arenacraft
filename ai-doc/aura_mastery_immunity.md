# Aura Mastery / interrupt-immunity leak (suspected)

Status: **unresolved, low priority**. Reported long ago, could not be reproduced
with the simple sequence (see below). Documented so a future investigation does
not redo the research from scratch.

Player report: it was possible to end up with **kick/interrupt immunity**
"somehow" via Aura Mastery (`31821`), apparently without the immunity aura being
visible. Not urgent; may have been masked/fixed accidentally by an unrelated
change.

## How it is implemented in our tree

We still run the **old hardcoded AzerothCore handler**, not upstream's current
script-based one.

- `src/game/Spells/Auras/SpellAuras.cpp:1962-1979`, in
  `Aura::HandleAuraSpecificMods` (`SPELLFAMILY_PALADIN`):
  - `19746` (Concentration Aura) / `31821` (Aura Mastery), only for self-cast.
  - apply: if the other aura is present, `CastSpell(target, 64364)`.
  - remove: `RemoveAurasDueToSpell(64364, GetCasterGUID())`.
- `64364` = "Aura Mastery Immune", the hidden immunity aura. Its effect
  (`SPELL_AURA_MECHANIC_IMMUNITY`, interrupt bit) registers a raw entry in the
  target's immunity table (`src/game/Spells/Auras/SpellAuraEffects.cpp:4173-4178`,
  handler at `SpellAuraEffects.cpp:4249`).
- `src/game/Scripts/Spells/spell_paladin.cpp` only has the `19746` / `64364`
  enum constants (`SPELL_PALADIN_CONCENTRACTION_AURA`,
  `SPELL_PALADIN_AURA_MASTERY_IMMUNE`). **There is no AuraScript for `31821` or
  `64364` in our tree**, and no `spell_script_names` binding for them.

## Why it can leak (hypothesis)

The kick immunity is not the aura object; it is a `m_spellImmune[IMMUNITY_MECHANIC]`
entry. The aura only applies/removes it.

1. `Unit::ApplySpellImmune` (`src/game/Entities/Unit/Unit.cpp:13777`) **pushes a
   new entry on every apply** (except `spellId == 0`) and on remove erases only
   the **first** matching entry (`break`). Any apply-twice/remove-once sequence
   therefore leaves a permanent interrupt-immunity entry with no aura to show
   for it — matching the player report.
2. `Aura::HandleAuraSpecificMods` is not only called on real apply/remove: it is
   also called from `Aura::SetStackAmount` (`SpellAuras.cpp:1021-1052`) with
   `apply=false` then `apply=true` when an aura is refreshed/re-stacked
   (`onReapply == true`). The paladin branch ignores `onReapply`, so refreshing
   Concentration Aura re-enters the logic, tears down `64364` and re-casts it
   **from inside an in-progress aura refresh**.
3. That re-cast is a triggered `CastSpell` from inside aura bookkeeping, which is
   exactly the kind of re-entrancy where the aura object and the immunity entry
   can desync.
4. The removal branch only runs when the aura's caster is the target
   (`GetCasterGUID() != target->GetGUID() -> break`), so the same cleanup does
   not run for applications on other targets (party members under the aura); it
   relies on `64364`'s own area-aura target bookkeeping and on the owning aura's
   lifetime instead.
5. Removal is caster-GUID filtered (`RemoveAurasDueToSpell(64364, caster)`): if a
   `64364` instance ever carries a different caster GUID, removal silently
   no-ops.

Related but probably not the cause: `Unit::RemoveArenaAuras`
(`src/game/Entities/Unit/Unit.cpp:5512`) strips positive non-passive auras on
arena entry, so preparation-phase Aura Mastery is torn down there too.

## What upstream does now (for contrast)

Current AzerothCore removed the hardcoded case from `HandleAuraSpecificMods` and
moved it into `spell_paladin.cpp`:

- `spell_pal_aura_mastery` (`31821`): on apply always casts `64364`; on remove
  `RemoveOwnedAura(64364, GetCasterGUID())`.
- `spell_pal_aura_mastery_immune` (`64364`): area aura with `DoCheckAreaTarget`
  requiring `HasAura(19746, GetCasterGUID())`.

So the "is Concentration Aura up?" check is continuously re-evaluated by the
immunity aura itself, instead of being a one-shot apply/remove branch.

## Reproduction attempts

Simple version (one paladin) — **tested, did NOT reproduce** (Nov 2026):

1. Cast Concentration Aura.
2. Cast Aura Mastery.
3. Recast/spam Concentration Aura during the Aura Mastery window.
4. Let Aura Mastery expire.
5. Have someone kick/interrupt: still interrupted normally.

Party version (2v2/3v3) — **not tested yet, most likely candidate**:

1. Paladin puts up Concentration Aura, teammate stands in it.
2. Paladin casts Aura Mastery.
3. Inside the window, paladin toggles Concentration Aura -> Devotion Aura ->
   Concentration Aura a few times.
4. Let Aura Mastery expire.
5. Enemy kicks/interrupts the teammate (and the paladin) — check whether either
   still casts through it.

If it reproduces, check `64364`-related immunity bookkeeping (entries vs.
`HasAura(64364)`); a leftover entry with no aura confirms the hypothesis.

## Fix direction if it reappears

Port the two upstream scripts into `src/game/Scripts/Spells/spell_paladin.cpp`,
delete the `19746`/`31821` case from `Aura::HandleAuraSpecificMods`, and add the
`spell_script_names` bindings. Do not rebuild this in the hardcoded switch.
