# ISSUE 83 — Hunter Auto Shot: 0.5 s windup / low damage share

## Player reports

1. *"Auto Shot doesn't fire until the animation finishes, creating a delay when briefly stopping to shoot."*
2. *"on warmane for example its usually 25-35% of your dmg autoshots, but here is about 5% up to 6-7 max."*

Both reports share one root cause: **Auto Shot is being fired too rarely**, not for too little damage per shot.

---

## TL;DR

Our fork carries the upstream commit that added a 0.5 second windup to Hunter Auto Shot
(`f2ed6f791`, PR #19603). Upstream reverted that commit in `4a11fe0db` (PR #21653, March 2025)
because the windup was an early WotLK-Classic bug, not Wrath 3.3.5 behaviour. Our repo's
merge-base with the reference on the affected file is exactly `f2ed6f791`, and the revert is
absent from our history.

The stale code makes Hunter Auto Shot subject to:

- a mandatory 500 ms delay after starting Auto Shot,
- a mandatory 500 ms delay after moving,
- a mandatory 500 ms delay after casting any generic spell (Arcane Shot, Steady Shot, …),
- a complete block on firing while moving.

In an arena this collapses Auto Shot uptime, which explains both the perceived animation delay
and the drop from a Warmane-like 25–35% damage share to ~5–7%.

The fix is a two-condition change in `Unit::_UpdateAutoRepeatSpell()`.

---

## Evidence

### Our code (buggy, pre-revert)

`src/game/Entities/Unit/Unit.cpp`

```cpp
// Check "realtime" interrupts
if ((IsPlayer() && ToPlayer()->isMoving()) ||
    IsNonMeleeSpellCast(false, false, true, spellProto->Id == HUNTER_AUTOSHOOT))
{
  // cancel wand shoot
  if (spellProto->Id != HUNTER_AUTOSHOOT)
    InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
  m_AutoRepeatFirstCast = true;
  return;
}

if (m_AutoRepeatFirstCast && getAttackTimer(RANGED_ATTACK) < 500)
{
  setAttackTimer(RANGED_ATTACK, 500);
}
```

### Reference core (fixed, current upstream)

`azerothcore-wotlk/src/server/game/Entities/Unit/Unit.cpp`

```cpp
// Check "realtime" interrupts
if ((IsPlayer() && ToPlayer()->isMoving() && spellProto->Id != HUNTER_AUTOSHOOT) ||
    IsNonMeleeSpellCast(false, false, true, spellProto->Id == HUNTER_AUTOSHOOT))
{
    // cancel wand shoot
    if (spellProto->Id != HUNTER_AUTOSHOOT)
        InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    m_AutoRepeatFirstCast = true;
    return;
}

// Apply delay (Hunter's autoshoot not affected)
if (m_AutoRepeatFirstCast && getAttackTimer(RANGED_ATTACK) < 500 && spellProto->Id != HUNTER_AUTOSHOOT)
{
    setAttackTimer(RANGED_ATTACK, 500);
}
```

Only two conditions differ, both gated on `HUNTER_AUTOSHOOT` (spell id 75):

1. the movement interrupt no longer applies to Hunter Auto Shot,
2. the 500 ms minimum attack timer no longer applies to Hunter Auto Shot.

### Upstream history

| Commit | Date | Description | In our fork? |
| --- | --- | --- | --- |
| `f2ed6f791` | 2024-08-26 | `fix (Core/Unit): add AutoRpeatSpell delay to Auto Shot as well (#19603)` | **Yes** (ancestor of HEAD) |
| `4a11fe0db` | 2025-03-08 | `Core/Unit: Revert AutoRepeatSpell delay application to Auto Shot (#19603)` / PR #21653 | **No** |

`git merge-base HEAD f2ed6f791` in the reference resolves to `f2ed6f791` itself, confirming our
fork froze at the bug-introducing commit and never received the revert.

Upstream rationale (PR #21653, authored by the same contributor who added the delay):
> "This reverts commit f2ed6f7. It was an early wotlk classic bug…"

Related reports:

- ChromieCraft #7913 — *"[Hunter] 0.5s delay before auto-shot (not Wrath blizz-like)"*:
  "when swing timer is up, auto shot still takes 0.5s to cast. In Wrath this delay should be
  removed. Auto shot will immediately fire if stopped for even a millisecond."
- Blizzard Wrath forums: Auto Shot should fire whenever the character is standing still,
  regardless of how long; "stutter stepping" was the intended technique.
- TrinityCore #19546: confirms the movement/windup discussion and that the windup is not
  intended for Wrath.

### Why the damage share collapses

`m_AutoRepeatFirstCast` is set in several places, and in our fork Hunter Auto Shot is subject to
all of them:

- whenever the player is moving (the function also returns before the shot check, so no shots
  fire while moving),
- when a generic spell is started (Arcane Shot, Steady Shot, Serpent Sting, etc.),
- when Auto Shot itself is started (`CURRENT_AUTOREPEAT_SPELL`).

Each flag set turns into a 500 ms ranged-timer floor. In an arena rotation that means the hunter
is repeatedly paying 500 ms before the next Auto Shot, on top of being unable to fire while
moving. Over a match this can easily reduce Auto Shot count by 4–5×, matching the observed
5–7% vs 25–35% damage share.

---

## Ruled out

These were checked and are **not** the cause:

- **Per-shot damage math.** `spell_dbc`/`Spell.dbc` for spell 75 is a normal auto-repeat ranged
  `SPELL_EFFECT_WEAPON_DAMAGE` spell. `CalculateDamage`, `CalculateMinMaxDamage`, ammo DPS,
  ranged attack power, agility scaling, and `MeleeDamageBonusDone` are identical to the
  reference.
- **Proc/flags/combat log.** `Spell::prepareDataForTriggerSystem` sets the same
  `PROC_FLAG_DONE_RANGED_AUTO_ATTACK` as upstream; the damage log path is the same.
- **`CalcCastTime` +500.** That bonus is explicitly skipped for auto-repeat spells
  (`SpellInfo.cpp`), so it is not adding the delay.
- **DB override.** The `spell_dbc` row for 75 exists but is harmless (`CastingTimeIndex = 1`,
  `InterruptFlags = 1`); it does not change the weapon-damage effect or the server-side 500 ms
  timer.
- **`resetAttackTimer` debt behaviour.** Our fork still has the old
  `std::min(timer + time, time)` variant, but that can only make swings slightly faster after
  lag, not slower.

The only hardcoded knob is the literal `500`. There is no config or DB option for the Auto Shot
windup.

---

## Fix

Applied in `src/game/Entities/Unit/Unit.cpp`, `Unit::_UpdateAutoRepeatSpell()`:

1. Movement check is skipped for `HUNTER_AUTOSHOOT`.
2. The 500 ms minimum ranged attack timer is skipped for `HUNTER_AUTOSHOOT`.

This restores pre-bug behaviour: the first shot fires immediately, shots fire immediately after
stopping or after weaving another ability, and (like upstream/Warmane) Auto Shot may fire while
moving.

**Note:** if strict "must stop to shoot" Wrath behaviour is preferred later, only condition 2
needs to stay; reverting condition 1 would re-enable the movement gate while keeping the windup
fix.

**Optional related backport (not applied):** upstream `9d802dcd0` resets
`BASE_ATTACK`/`OFF_ATTACK` timers after a ranged shot. Our fork lacks it; it is a separate
blizzlike timer correction and does not materially affect Hunter Auto Shot share.

---

## Verification plan

1. `zig build ac` to compile.
2. In-game with a hunter:
   - standing still, first Auto Shot fires as soon as the swing timer is ready (no ~0.5 s
     windup),
   - stutter-step: stopping for a tick fires immediately,
   - Auto Shot also fires while moving,
   - after Arcane/Steady Shot, the next Auto Shot is not gated by an extra 500 ms.
3. Dummy/arena session: compare Auto Shot share before vs after; expected to move from ~5–7%
   toward the Warmane-like 20–35%.
4. No SQL/migration change is involved.

---

## Files touched

- `src/game/Entities/Unit/Unit.cpp` — `Unit::_UpdateAutoRepeatSpell()` (two conditions).
