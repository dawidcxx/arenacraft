# Known bugs (WoW 3.3.5 mechanics)

Living list of gameplay/mechanic bugs found by reading the code. Each entry has a
location, a concrete repro, and a suggested fix. Nothing here is fixed yet unless
marked otherwise.

Status legend:

- **confirmed (code)** — the faulty code path was traced end to end; not yet
  reproduced in game.
- **needs in-game check** — the code looks suspicious, but retail 3.3.5 behavior
  or the exact repro still has to be verified.

Related, already documented investigations (not repeated here):

- Charge / Intercept / Feral Charge pathing — `ai-doc/research/CHARGE_PATHING_ISSUES.md`
- Server-side movement / "server moved me" — `ai-doc/research/MOVEMENT_CODE_REVIEW.md`

## Summary

| # | Area | Severity | Status |
|---|------|----------|--------|
| 1 | CC re-apply guard eats combo points (Kidney Shot) | High | confirmed (code) |
| 2 | CC re-apply guard drops the damage part of CC spells | Medium | confirmed (code) |
| 3 | CC re-apply guard sends "bounced" errors for proc-triggered CC | Low | needs in-game check |
| 4 | Arena dampening ignores pets and guardians | Medium | confirmed (code) |
| 5 | `.challenge` against your own party member puts the party on both teams | High | confirmed (code) |
| 6 | Post-teleport unroot hack clears legitimate roots/stuns | Medium | confirmed (code) |
| 7 | Arena pre-buffs skip pets | Low | confirmed (code) |
| 8 | Arena pre-buffs are cast with the receiver's talents | Low | confirmed (code) |
| 9 | Horn of Winter pre-buff expires early in the match | Low | confirmed (code) |
| 10 | Duel reset happens on accept, not on duel end | Low | confirmed (code) |
| 11 | `SMSG_ARENA_UNIT_DESTROYED` sent for every unit, incl. own team / totems | Low | needs in-game check |
| 12 | Grounding Totem redirect has no LoS check | Low | needs in-game check |

---

## 1. CC re-apply guard eats combo points

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

- **Where:** `Spell.cpp:3208` (guard) → `Spell.cpp:2777` (`m_damage = 0` on any non-NONE result).
- **What:** the guard turns the **whole** hit into a miss, not just the aura. Any CC spell that
  also deals damage loses its damage when it bounces: Concussion Blow, Dragon's Breath,
  Frost Nova, Shockwave, Deep Freeze (immune-target damage), Gouge, Intercept, etc.
- **Repro:** mage casts Frost Nova, then a second Frost Nova (e.g. Cold Snap) within the first half
  of the root: no damage, combat log shows a miss.
- **Fix:** drop only the aura effects (`effectMask &= ~aura_effmask`) instead of returning
  `SPELL_MISS_MISS` for the whole spell.

## 3. CC re-apply guard sends "bounced" errors for proc-triggered CC

- **Where:** `Spell.cpp:3208`.
- **What:** the guard also runs for triggered/proc CC (Impact, Improved Hamstring, Seal of
  Justice, Blackout, Charge stun, ...). Each blocked proc sends
  `SPELL_FAILED_AURA_BOUNCED` to the caster, so the player gets a red error for a spell they
  never cast. For AoE CC (Psychic Scream, Howl of Terror, Frost Nova) it is sent once per
  bounced target.
- **Fix:** skip `SendCastResult` when `IsTriggered()`, and send it at most once per cast.

## 4. Arena dampening ignores pets and guardians

- **Where:** `Arena::PreUpdateImpl` (`src/game/Battlegrounds/Arena.cpp:181`).
- **What:** the +10%/+20% damage stacks of 29659 are applied only to `GetPlayers()`.
  Hunter / warlock / DK pets, Shadowfiend, Gargoyle, Army of the Dead, Mirror Images, Feral
  Spirits, Treants keep base damage. Pet-heavy comps (BM hunter, affliction/demo lock, unholy
  DK) get a smaller share of the dampening bonus than everyone else.
- **Fix:** also apply the aura to `player->GetPet()` and the player's controlled units
  (`m_Controlled`), and to pets summoned after dampening started.

## 5. `.challenge` against your own party member puts the party on both teams

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
