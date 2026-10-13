# Known bugs (WoW 3.3.5 mechanics)

Living list of gameplay/mechanic bugs found by reading the code. Each entry has a
location, a concrete repro, and a suggested fix. Nothing here is fixed yet unless
marked otherwise. Once a fix is merged, move the entry's status to **fixed** (or drop
the entry) in the same change.

Status legend:

- **confirmed (code)** — the faulty code path was traced end to end; not yet
  reproduced in game.
- **needs in-game check** — the code looks suspicious, but retail 3.3.5 behavior
  or the exact repro still has to be verified.
- **fix in review (`branch`)** — a fix exists on the named branch and is waiting
  for review/merge.

Related, already documented investigations (not repeated here):

- Charge / Intercept / Feral Charge pathing — `ai-doc/research/CHARGE_PATHING_ISSUES.md`
- Server-side movement / "server moved me" — `ai-doc/research/MOVEMENT_CODE_REVIEW.md`

## Summary

| # | Area | Severity | Status |
|---|------|----------|--------|
| 1 | CC re-apply guard eats combo points (Kidney Shot) | High | fix in review (`fix/cc-reapply-guard`) |
| 2 | CC re-apply guard drops the damage part of CC spells | Medium | fix in review (`fix/cc-reapply-guard`) |
| 3 | CC re-apply guard sends "bounced" errors for proc-triggered CC | Low | fix in review (`fix/cc-reapply-guard`) |
| 4 | Arena dampening ignores pets and guardians | Medium | fix in review (`fix/dampening-pets`) |
| 5 | `.challenge` against your own party member puts the party on both teams | High | fix in review (`fix/challenge-same-party`) |
| 6 | Post-teleport unroot hack clears legitimate roots/stuns | Medium | fix in review (`fix/teleport-unroot-hack`) |
| 7 | Arena pre-buffs skip pets | Low | fix in review (`fix/prebuffs-pets`) |
| 8 | Arena pre-buffs are cast with the receiver's talents | Low | confirmed (code) |
| 9 | Horn of Winter pre-buff expires early in the match | Low | confirmed (code) |
| 10 | Duel reset happens on accept, not on duel end | Low | confirmed (code) |
| 11 | `SMSG_ARENA_UNIT_DESTROYED` sent for every unit, incl. own team / totems | Low | needs in-game check |
| 12 | Grounding Totem redirect has no LoS check | Low | needs in-game check |
| 13 | Ebon Gargoyle deals about half of its retail damage | Medium | fix in review (`fix/gargoyle-damage`) |
| 14 | Stealth broken by friendly spells and by fully absorbed DoT ticks | Medium | needs in-game check |
| 15 | Vanish/Shadowmeld only absorbs in-flight spells for 500 ms, never instant ones | Medium | needs in-game check |
| 16 | Deep Freeze bonus damage procs on temporarily stun-immune players | Medium | needs in-game check |
| 17 | Retaliation proc may use the wrong damage spell | Medium | needs in-game check |
| 18 | Recasting Cyclone/Banish on an already affected target may refresh it | Low | needs in-game check |
| 19 | Shaman totems stacked in one point when dropped mid-air or off the navmesh | Medium | fix in review (`fix/totem-placement`) |

---

## 1. CC re-apply guard eats combo points

- **Status:** fix in review — `fix/cc-reapply-guard`: pure CC is rejected in `Spell::CheckCast` before power, combo points or cooldown are spent.

- **Where:** `Spell::DoSpellHitOnUnit` (`src/game/Spells/Spell.cpp:3208`), combo point
  handling in `Spell::DoAllEffectOnTarget` (`Spell.cpp:2789`) and `Spell.cpp:4442`.
- **What:** the arenacraft guard ("same caster cannot re-apply CC of the same DR group
  while the existing aura has > 50% left", see `ai-doc/server_side_balance.md`) returns
  `SPELL_MISS_MISS` from `DoSpellHitOnUnit`. By then the original `missInfo` is already
  `SPELL_MISS_NONE`, so the "do not take combo points on miss" branch (`Spell.cpp:2789`)
  never runs and `ClearComboPoints()` (`Spell.cpp:4442`) still consumes them. Energy and
  the cooldown are spent too.
- **Repro:** rogue opens with Cheap Shot (4 s), builds 5 CP, and casts Kidney Shot within the
  first 2 s. Both are `DIMINISHING_CONTROLLED_STUN` from the same caster, so Kidney Shot
  "bounces": no stun, 5 CP + 25 energy + cooldown lost.
- **Fix:** reject the re-apply in `Spell::CheckCast` (before any cost is paid) instead of at
  hit time, or at least reset `m_needComboPoints` / refund power when the guard triggers.

## 2. CC re-apply guard drops the damage part of CC spells

- **Status:** fix in review — `fix/cc-reapply-guard`: only the aura effects are dropped at hit time, the damage still lands.

- **Where:** `Spell.cpp:3208` (guard) → `Spell.cpp:2777` (`m_damage = 0` on any non-NONE result).
- **What:** the guard turns the **whole** hit into a miss, not just the aura. Any CC spell that
  also deals damage loses its damage when it bounces: Concussion Blow, Dragon's Breath,
  Frost Nova, Shockwave, Deep Freeze (immune-target damage), Gouge, Intercept, etc.
- **Repro:** mage casts Frost Nova, then a second Frost Nova (e.g. Cold Snap) within the first half
  of the root: no damage, combat log shows a miss.
- **Fix:** drop only the aura effects (`effectMask &= ~aura_effmask`) instead of returning
  `SPELL_MISS_MISS` for the whole spell.

## 3. CC re-apply guard sends "bounced" errors for proc-triggered CC

- **Status:** fix in review — `fix/cc-reapply-guard`: the error is sent only for non-triggered casts and only for the main target.

- **Where:** `Spell.cpp:3208`.
- **What:** the guard also runs for triggered/proc CC (Impact, Improved Hamstring, Seal of
  Justice, Blackout, Charge stun, ...). Each blocked proc sends
  `SPELL_FAILED_AURA_BOUNCED` to the caster, so the player gets a red error for a spell they
  never cast. For AoE CC (Psychic Scream, Howl of Terror, Frost Nova) it is sent once per
  bounced target.
- **Fix:** skip `SendCastResult` when `IsTriggered()`, and send it at most once per cast.

## 4. Arena dampening ignores pets and guardians

- **Status:** fix in review — `fix/dampening-pets`: dampening is applied to every alive unit in the player's `m_Controlled` on each update (so later summons are covered too), and 29659 is marked unsaveable.

- **Where:** `Arena::PreUpdateImpl` (`src/game/Battlegrounds/Arena.cpp:181`).
- **What:** the +10%/+20% damage stacks of 29659 are applied only to `GetPlayers()`.
  Hunter / warlock / DK pets, Shadowfiend, Gargoyle, Army of the Dead, Mirror Images, Feral
  Spirits, Treants keep base damage. Pet-heavy comps (BM hunter, affliction/demo lock, unholy
  DK) get a smaller share of the dampening bonus than everyone else.
- **Fix:** also apply the aura to `player->GetPet()` and the player's controlled units
  (`m_Controlled`), and to pets summoned after dampening started.

## 5. `.challenge` against your own party member puts the party on both teams

- **Status:** fix in review — `fix/challenge-same-party`: `.challenge` is refused when both players are in the same group. The cooldown note under "Related" is not addressed.

- **Where:** `ChallengeCommand::HandleChallenge` (`src/game/Arenacraft/Challenge.cpp:101`, `:188-189`).
- **What:** nothing checks that `enemy` is outside the challenger's group. If A and B are in one
  party and A does `.challenge B`, both `CollectSide` calls return the same 2-player party, the
  size check passes (2 == 2), and the same group is queued and invited on **both** teams.
- **Repro:** two friends in a party; one types `.challenge <friend>` to get a 1v1.
- **Fix:** refuse when `enemy->GetGroup() == challenger->GetGroup()` (or offer a real 1v1 by
  building each side from the single player when both are in the same party).
- **Related:** no per-player cooldown on `.challenge`, so a player can repeatedly send arena
  invites to anyone who has not ignored them.

## 6. Post-teleport unroot hack clears legitimate roots/stuns

- **Status:** fix in review — `fix/teleport-unroot-hack`: the delayed unroot is skipped while a root/stun aura is active or the player is on a vehicle.

- **Where:** `WorldSession::HandleMoveWorldportAck` (`src/game/Handlers/MovementHandler.cpp:272`).
- **What:** 1 s after **every** far teleport the server unconditionally clears
  `UNIT_STATE_ROOT | UNIT_STATE_STUNNED` and sends an unroot, without checking whether a root or
  stun aura is actually active. Anything that legitimately roots/stuns the player in that 1 s
  window (a CC landing right after a zone-in, a scripted stun, being on a vehicle/transport)
  ends up with the aura still present but the unit not rooted: server state and aura state
  disagree until the aura is removed.
- **Fix:** only clear the states / send the unroot when
  `!HasAuraType(SPELL_AURA_MOD_ROOT) && !HasAuraType(SPELL_AURA_MOD_STUN)` (and not on a vehicle).

## 7. Arena pre-buffs skip pets

- **Status:** fix in review — `fix/prebuffs-pets`: `arenacraft::ApplyArenaPreBuffsToPet` runs after the pet has loaded (`Pet::LoadPetFromDB`, after `RemoveArenaAuras`) and for new pets (`Player::SummonPet`), during the preparation phase only.

- **Where:** `ArenaPreBuffs::OnBattlegroundAddPlayer` (`src/game/Arenacraft/PreBuffs.cpp:106`).
- **What:** only players are buffed. Pets enter the arena without Mark of the Wild, Fortitude,
  Spirit, Shadow Protection, Arcane Intellect, so a hunter/warlock must spend preparation
  time asking for single-target buffs on the pet.
- **Fix:** cast the non-self-only buffs on `player->GetPet()` as well (after the pet is
  re-summoned on arena entry).

## 8. Arena pre-buffs are cast with the receiver's talents

- **Where:** `ApplyClassBuffsToPlayer` (`PreBuffs.cpp:83`).
- **What:** buffs are cast as `target->CastSpell(target, ...)`, i.e. by the receiving
  player. Talent-scaled values (Improved Power Word: Fortitude, Improved Mark of the Wild,
  Brambles for Thorns, ...) come from the receiver, not from the class that provides the buff.
  A priest with Improved Fortitude still gives the base value; a non-priest receives it at
  base value too. Consistent, but different from a real pre-buff.
- **Fix:** if the provider is already on the map, cast from the provider (`provider->CastSpell(target, ...)`);
  otherwise keep the current fallback.

## 9. Horn of Winter pre-buff expires early in the match

- **Where:** `PreBuffs.cpp:23`.
- **What:** Horn of Winter (57623) lasts 2 min. Pre-buffs are applied on arena entry, and the
  preparation phase is about 1 min, so the buff is gone about 1 min after the gates open. All the
  other pre-buffs last 30-60 min. DKs refresh it anyway, but non-DK teammates lose it mid-fight.
- **Fix:** drop it from the list, or re-apply it at gate open (`STATUS_IN_PROGRESS`).

## 10. Duel reset happens on accept, not on duel end

- **Where:** `WorldSession::HandleDuelAcceptedOpcode` (`src/game/Handlers/DuelHandler.cpp:54-60`).
- **What:** in the starting zone (3523), HP/power/arena cooldowns are reset when the duel is
  **accepted**. After the duel ends, both players keep their low HP and spent cooldowns. Any
  pair of players can also use "duel + flee" as an instant full HP/mana/cooldown reset.
  Debuffs/DoTs from the previous duel are not cleared either.
- **Fix:** also reset in `Player::DuelComplete`, and clear harmful auras applied by the
  opponent.

## 11. `SMSG_ARENA_UNIT_DESTROYED` sent for every unit

- **Where:** `WorldObject::DestroyForNearbyPlayers` (`src/game/Entities/Object/Object.cpp:3002`).
- **What:** the packet goes out for any unit leaving the world (totems, guardians, temporary
  summons, teammates), not only for arena opponents. Arena frame addons (Gladius, sArena) may
  react to it for units that are not opponents at all.
- **Check:** confirm with an addon that totem/guardian despawns and teammate deaths do not
  blank arena frames. If they do, limit the packet to players and pets of the other team.

## 12. Grounding Totem redirect has no LoS check

- **Where:** `Unit::GetMagicHitRedirectTarget` (`src/game/Entities/Unit/Unit.cpp:11294`,
  `IsWithinLOSInMap(magnet)` commented out).
- **What:** a spell can be grounded by a totem the caster has no line of sight to (e.g.
  the totem is behind a pillar while the shaman is in LoS). Also, the totem is killed only after a
  100 ms queue / travel-time delay, so several spells cast in the same window can all be eaten
  by one totem.
- **Check:** compare with retail 3.3.5 behavior before changing anything. This is long-standing
  AzerothCore behavior, not an arenacraft change.

## 13. Ebon Gargoyle deals about half of its retail damage

- **Status:** fix in review — `fix/gargoyle-damage`: the gargoyle's spell power is 33.3% of the
  DK's attack power +4% per Impurity rank (40% at 5/5), and Gargoyle Strike takes 100% of it
  (coefficient override in `SpellMgr::LoadSpellBonuses`), so a strike scales with 0.4 AP.

- **Where:** `spell_dk_pet_scaling::CalculateSPAmount` (`src/game/Scripts/Spells/spell_dk.cpp`),
  `spell_bonus_data` row for 51963 (direct coefficient 0.453), `spell_pet_dk_gargoyle_strike`
  (`src/game/Scripts/Pet/pet_dk.cpp`, base 140 at level 80).
- **What:** the gargoyle gets 33% of the DK's attack power as spell power (40% with any Impurity
  rank), and Gargoyle Strike then gets only 0.453 of that spell power. The two cuts stack to
  ~0.15 AP per strike (~0.18 with Impurity). Retail 3.3.5 testing (3.1 era, quoted in
  AzerothCore issue #11387) puts it at 0.4 AP per strike.
- **Example:** DK with 6000 AP, 5/5 Impurity: ~1230 per strike now, ~2540 with 0.4 AP (before
  resilience and partial nature resists).
- **Not a bug:** attack power and haste are snapshotted at summon (correct for 3.3.5, see
  AzerothCore issue #27790); haste does shorten the Gargoyle Strike cast; partial resists on
  nature damage make individual strikes vary.

## Upstream (AzerothCore) fixes reviewed — 2026-10-13

AzerothCore commits after our fork point (2024-12-22) were checked against our
code. Fixes already present (often via the `backport:` commits) or areas we
changed on purpose are not listed.

- **Ported, in review:** `fix/security-backports` (13 crash / use-after-free /
  packet-validation / money fixes, one commit each, upstream PR in the message)
  and `fix/pvp-backports` (Hand of Freedom vs Intimidating Shout, Chains of Ice
  Frost Fever on immune targets, Bone Shield 2 s charge delay, fall damage
  threshold, fall info reset on near teleport, guardians excluded from party/raid
  target search).
- **Deliberately not ported:**
  - movement root safeguards that drop client packets (azerothcore#23117 part) —
    we removed those drops on purpose, see `ai-doc/research/MOVEMENT_CODE_REVIEW.md`
    Addendum 2;
  - fear pathing (azerothcore#22451, #24892) — we have our own flee cliff
    avoidance (`072cf5ac7`);
  - movement order counters / async speed ack (azerothcore#23015, #23371,
    #23465) — large protocol rework, partially backported already;
  - trade rework (azerothcore#24710) and anti-DoS opcode table
    (azerothcore#21502) — large refactors, the latter moves config into the DB.

## 14. Stealth broken by friendly spells and by fully absorbed DoT ticks

- **Where:** stealth removal in `Spell::DoSpellHitOnUnit` / aura procs,
  `AuraEffect::HandlePeriodicDamageAurasTick` and
  `HandlePeriodicHealthLeechAuraTick` (`procEx` always has NORMAL/CRITICAL_HIT).
- **What:** upstream fixed stealth charges being consumed by friendly/positive
  procs (azerothcore#24834, #24875, #24898) and a periodic tick that was fully
  absorbed still counting as a hit (azerothcore#24975). Our proc system is the
  older one, so the upstream patch does not apply as is.
- **Check:** rogue in stealth with a DoT absorbed by Power Word: Shield; friendly
  priest casting on a stealthed rogue.

## 15. Vanish/Shadowmeld spell absorption window

- **Where:** `Spell::DoAllEffectOnTarget`, "Xinef: absorb delayed projectiles for 500ms".
- **What:** in-flight spells are only absorbed if they land within 500 ms of the
  Sanctuary, area spells are excluded, and instant spells landing right after
  Vanish are not absorbed. Upstream (azerothcore#24937) absorbs every projectile
  launched before the Sanctuary and instant hostile spells for 400 ms after it.
- **Check:** compare with retail 3.3.5 before changing — this decides whether
  Vanish dodges a Frostbolt/Chaos Bolt in flight.

## 16. Deep Freeze bonus damage on stun-immune players

- **Where:** spell 71761 (Deep Freeze Immunity State) has no proc check.
- **What:** the 71757 damage proc is meant for targets permanently immune to
  stuns (bosses). Upstream (azerothcore#26168) restricts it to creatures with a
  template stun immunity; here it can proc on players who are only temporarily
  stun-immune (Bladestorm, Divine Shield, ...).
- **Fix:** port the `spell_mage_deep_freeze_immunity_state` AuraScript (needs a
  `spell_script_names` binding for 71761).

## 17. Retaliation damage spell

- **Where:** `spell_warr_retaliation` (`src/game/Scripts/Spells/spell_warrior.cpp`),
  `SPELL_WARRIOR_RETALIATION_DAMAGE = 22858`.
- **What:** upstream (azerothcore#24868) switched to 20240 and casts from the
  warrior onto the attacker, citing self-damage and lost charges. That change was
  made on top of their newer proc system; TrinityCore 3.3.5 still uses 22858.
- **Check:** use Retaliation in a duel and watch who takes the damage and how many
  charges are spent.

## 18. Recast of hostile self-immunity spells (Cyclone, Banish)

- **What:** upstream (azerothcore#27482) made a hostile spell that grants school
  immunity (Cyclone, Banish) report immunity when recast while its own aura is
  active, instead of refreshing. Our immunity code differs from upstream.
- **Check:** recast Cyclone on a cycloned target: it should fail as immune, not
  refresh the duration.

## 19. Shaman totems stacked in one point

- **Status:** fix in review — `fix/totem-placement`: `SelectTotemSlotPosition` gives
  every totem its own reachable spot (slot offset, then other directions, never
  within 1 yd of the shaman or another of its totems), searching from the ground
  below an airborne shaman.

- **Where:** `Spell::SelectImplicitCasterDestTargets` (`src/game/Spells/Spell.cpp`),
  `WorldObject::MovePositionToFirstCollision`, `Map::CheckCollisionAndGetValidCoords`.
- **What:** a totem's spot is the caster offset for its slot (3 yd). When the
  collision check fails the position is left unchanged, i.e. the shaman's own
  position, so every totem lands there (with no ground correction, possibly in the
  air). The check fails when the shaman is more than 7 yd from the navmesh
  (`PATHFIND_FARFROMPOLY_START` is not an accepted path type): falling from the
  Blade's Edge bridge or a Nagrand pillar, knocked up by Thunderstorm/Typhoon, or
  standing on a pillar top/ledge with no navmesh. A normal jump is not affected.
  Unchanged upstream.
- **Check:** drop totems (single and Call of the Elements) while falling from a
  height, while standing on a pillar edge, and with the back against a wall.

