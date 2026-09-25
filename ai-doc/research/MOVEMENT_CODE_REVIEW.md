# Movement authority review (server vs. client)

Status: **confirmed static scan** — nothing instrumented, nothing changed.
Audience: future contributor / AI picking this up.
Scope: *who* owns player movement in this core, not *how good* the paths are.
For charge/pathfinding quality specifically, see `CHARGE_PATHING_ISSUES.md`.
§1–8 are the original scan; §9–11 are a follow-up addendum: cross-check against
vmangos (reference core) and a phased plan targeting **movement smoothness under
all circumstances** (our actual end goal, which the original scan only half-covers).

## Context

- The movement core is **upstream AzerothCore, unmodified by Arenacraft**. The
  only fork commit touching `src/game/Movement/` or `Handlers/MovementHandler.cpp`
  is `c3644ec10 feat: fully migrate to zig-build`; the content diff of that
  commit against its parent for those files is **0 logic lines** (path move
  only). Same for `PlayerUpdates.cpp`, `WorldSession.cpp`, `WorldSocket.cpp`;
  `Unit.cpp`/`Player.cpp` differ only by a `uint32` cast and a blank comment.
- Upstream base is AzerothCore ~Dec 2024 (last non-fork author commit
  `564d7b6df`, 2024-12-22).
- Conclusion: any movement behaviour described here is inherited core
  behaviour, not something Arenacraft deliberately added.

## Verdict

The working hypothesis — "too much movement is attempted server-side
authoritative, and the server nudges/corrects players (seen as teleporting)" —
is **largely false for ordinary locomotion**. Normal player movement is already
**client-authoritative**: the server relays the client's own `MovementInfo` to
nearby players and adopts it, with no position reconciliation, no distance/speed
simulation, and no correction packet sent back to the mover.

What *is* server-authoritative is a small, enumerable set of **event-driven
overrides** (spells, CC, teleport, under-map, transport/vehicle) plus two
network-level anti-cheat mechanisms. The "passive anticheat" hook system that
would do movement validation is **present but has no implementation**, so it is
currently inert.

## 1. Normal movement is client-authoritative

Path, with references:

1. `WorldSession::HandleMovementOpcodes` — `src/game/Handlers/MovementHandler.cpp:344`.
2. Read the client's own `MovementInfo` — `:379-381` (`ReadMovementInfo`,
   `src/game/Server/WorldSession.cpp:947`).
3. Light sanity checks only:
   - guid must match the mover — `:362-370`;
   - coords must be finite / in-map (`Position::IsPositionValid` →
     `Acore::IsValidMapCoord`, `src/game/Entities/Object/Position.cpp:171` and
     `src/game/Grids/GridDefines.h:200-206`) — `:393-402`;
   - drop packets while a **server spline** is active — `:404-408` (see §3.B);
   - while `UNIT_FLAG_DISABLE_MOVE`, keep the stored position and drop the move
     — `:410-432`;
   - transport bookkeeping — `:434-509`.
4. Rewrite the movement timestamp via the time-sync delta — `:563-574`.
5. **Relay verbatim** to nearby players and store — `:576-580`:
   `WriteMovementInfo(&data, &movementInfo); mover->SendMessageToSet(&data, _player); mover->m_movementInfo = movementInfo;`
6. Adopt the client position — `mover->UpdatePosition(movementInfo.pos)` at `:598`.
   - `Player::UpdatePosition` (`src/game/Entities/Player/PlayerUpdates.cpp:1049`)
     → `Unit::UpdatePosition` (`src/game/Entities/Unit/Unit.cpp:20446`) →
     `Map::PlayerRelocation` (`src/game/Maps/Map.cpp:1012`) → `Relocate()`.
   - **No Z/ground clamping** happens on this path. `UpdateAllowedPositionZ`
     (`src/game/Entities/Object/Object.cpp:1549`) is *not* called here — it is
     used by movement generators / spawn placement only.

Consequences: the server does not simulate the player, does not detect speed or
distance anomalies, and does not send a corrective position to the moving
client. From a movement-authority standpoint the client is the source of truth.

## 2. The passive anticheat is inert (key finding)

The core defines a hook family intended for a passive anti-cheat module:

- Declarations: `src/game/Scripting/ScriptDefines/PlayerScript.h:980-987`
  (`AnticheatSetCanFlybyServer`, `AnticheatSetUnderACKmount`,
  `AnticheatSetRootACKUpd`, `AnticheatSetJumpingbyOpcode`,
  `AnticheatUpdateMovementInfo`, `AnticheatHandleDoubleJump`,
  `AnticheatCheckMovementInfo`).
- Dispatch: `src/game/Scripting/ScriptDefines/PlayerScript.cpp:1000-1037`
  (`ScriptMgr::Anticheat*`).
- Call sites in the packet path: `MovementHandler.cpp:397,418,441,456` (position
  rejections), `:517,527` (jump/land), `:549` (double jump),
  `:557` (`AnticheatCheckMovementInfo` before applying a move), plus assorted
  spell/aura call sites.

**No script implements any of them** (searched all of `src/` and `apps/`;
`git log --all --full-history -- '*nticheat*'` is empty; the only module removal
commit `e4b457691` is unrelated). The base virtuals are no-ops and the bool
hooks default to `true`:

- `AnticheatHandleDoubleJump` → `true` (`PlayerScript.h:986`) so no kick;
- `AnticheatCheckMovementInfo` → `true` (`PlayerScript.h:987`) so every move is
  accepted.

Therefore there is currently **zero position/distance/speed anti-cheat**. The
only live validation in the movement opcode handler is guid + map-coord validity
+ `UNIT_FLAG_DISABLE_MOVE` + the spline lock. Do not let a future contributor
drop a `mod-anticheat`-style module in without knowing this changes the whole
model (that is precisely where "server adjusts/teleports me" reports come from).

## 3. Server-authoritative movement inventory (the whole list)

| # | Mechanism | Where | Player-visible effect |
|---|---|---|---|
| A | Under-map: BG safe-teleport, else void damage → death | `MovementHandler.cpp:607-628`; overrides `BattlegroundDS.cpp:171`, `RV.cpp:178`, `BE.cpp:43`, `NA.cpp:39`, `RL.cpp:40` | **Hard teleport / void death** — top "teleport" suspect |
| B | Spline lock: client move packets discarded while a server spline runs | `MovementHandler.cpp:404-408`; `Unit.cpp:561-620` | Input ignored during charge/knockback → rubber-band |
| C | Forced movement-state flags (root/stun/fear/confuse, can-fly/hover/water-walk/feather-fall) | `Unit.cpp:18538,18586,18668,18700`; `Player.cpp:16174,16196,16217,16238` | Client state snapped by server packets |
| D | `ReadMovementInfo` strips "illegal" flags before relay/store | `WorldSession.cpp:1008-1061` | Server relays/stores different flags than the client has → divergence |
| E | Forced speed-change ACK: reset or **kick** | `MovementHandler.cpp:632-739` (kick `:736`) | Kick on echoed-speed mismatch |
| F | Over-speed ping kick (`MaxOverspeedPings`, default 2) | `WorldSocket.cpp:767-812`; config `World.cpp:1081` | Kick on rapid pings |
| G | Warden memory scanner (default on; Win/OSX only) | `World.cpp:1505`; gate `WorldSocket.cpp:627-635` | Anti-cheat, not movement |
| H | Time-sync clock delta rewrites relayed movement timestamps | `MovementHandler.cpp:563-574,914-984` | Relay jitter / interpolation snap for observers |
| I | Knockback ACK re-broadcast from server state | `MovementHandler.cpp:786-820` | Server-authored knockback |
| J | Teleport flow (worldport / near) | `MovementHandler.cpp:50,274` | Expected |

Notes and exactness:

- **A.** `MovementHandler.cpp:607` compares the client Z against
  `Map::GetMinHeight`. If below and the battleground handles it, the player is
  `NearTeleportTo`'d to a fixed spot (e.g. `BattlegroundNA.cpp:41`,
  `BattlegroundRL.cpp:42`, `BattlegroundBE.cpp:45`, `BattlegroundDS.cpp:173`,
  `BattlegroundRV.cpp:180`). Otherwise the player takes `DAMAGE_FALL_TO_VOID`
  and is killed (`:613-618`), then sent to the closest graveyard on next move
  (`:619-627`). Arenacraft's active arena pool is Nagrand / Ruins of Lordaeron /
  Blade's Edge (`BattlegroundMgr.cpp`, committed in `656f9efb6`), so all live
  arenas have the safe-teleport override — under-map in an arena is a teleport,
  not a death. This is the single most plausible source of a legitimate
  "I got teleported" report.
- **B.** `if (!mover->movespline->Finalized()) { ...; return; }`. Server-authored
  splines are advanced per tick by `Unit::UpdateSplineMovement` (`Unit.cpp:561`,
  called from `Unit::Update` at `:501`). For normal client locomotion no server
  spline is active, so the guard is a no-op; it only bites while the server is
  driving the player (charge/intercept, knockback, some scripted moves, taxi).
- **C.** These are the real "server tells the client where/how to move" packets:
  `SMSG_SPLINE_MOVE_ROOT/UNROOT` / `SMSG_FORCE_MOVE_ROOT/UNROOT`
  (`Unit.cpp:18622-18651`), `SMSG_MOVE_SET_CAN_FLY`/`UNSET`
  (`Player.cpp:16184-16192`), `SMSG_MOVE_SET_HOVER` (`:16205-16213`),
  water-walk (`:16226-16234`), feather-fall (`:16238+`). Separately,
  `Player::SetClientControl` (`Player.cpp:13081`) sends
  `SMSG_CLIENT_CONTROL_UPDATE` to **revoke client control** while feared/confused
  (`Unit.cpp:18694`) or on a vehicle — during that window the client cannot move
  itself at all. Expected in arena (roots/stuns/fears), but each is
  client-visible snapping.
  - **Fix applied:** `Unit::SetFeared` / `Unit::SetConfused` used to call
    `SetClientControl` only when `m_movedByPlayer` (charmer/vehicle); a feared
    player therefore kept client control and could fight the server flee spline,
    showing up as *walking* instead of running. The `else` now revokes/restores
    the player's own control, matching what `MovementHandler` already resends
    after a teleport for FLEEING/CONFUSED.
- **D.** Release build silently removes: `MOVEMENTFLAG_ROOT`; `HOVER` without
  `SPELL_AURA_HOVER`; `ASCENDING+DESCENDING`; `LEFT+RIGHT`;
  `STRAFE_LEFT+RIGHT`; `PITCH_UP+PITCH_DOWN`; `FORWARD+BACKWARD`;
  `WATERWALKING` without aura; `FALLING_SLOW` without aura;
  `FLYING|CAN_FLY` for non-GM without fly aura; `FALLING` when
  `CAN_FLY|DISABLE_GRAVITY`; `SPLINE_ENABLED` when no initialized spline. Debug
  builds only log (`#ifdef ACORE_DEBUG`, `WorldSession.cpp:985-1002`). The
  stripped value is what gets relayed/stored — the client itself is not
  corrected, so a legit-but-unrecognized state (aura timing/edge cases) can make
  the server's view differ from the client's.
- **E.** Only runs for **server-initiated** forced speed changes
  (`m_forced_speed_changes`), and is skipped while on a transport. If the echoed
  speed is lower, `SetSpeed` re-forces the correct value (`:725-731`); if higher,
  the player is kicked (`:733-737`). Clients cannot set their own speed: the
  `MSG_MOVE_SET_*_SPEED` opcodes are `STATUS_NEVER` / `Handle_NULL`
  (`src/game/Server/Protocol/Opcodes.cpp:407-421`), so the ACK path is the only
  speed authority.
- **F.** `HandlePing` (`WorldSocket.cpp:767`): if a ping arrives less than
  `seconds(27)` after the previous one, `_OverSpeedPings` increments; above
  `MaxOverspeedPings` (`World.cpp:1081`, default 2) a player account is kicked
  (`:789-806`). This is a connection-level check, unrelated to movement.
- **G.** `Warden.Enabled` defaults `true` (`World.cpp:1505`); clients whose
  `account.OS` is not `Win`/`OSX` are **rejected at login** when Warden is on
  (`WorldSocket.cpp:627-635`). Warden is memory/hook scanning, not movement
  validation.
- **H.** `movementInfo.time` is rewritten by `_timeSyncClockDelta`
  (`MovementHandler.cpp:563-574`); the delta is estimated in
  `HandleTimeSyncResp`/`ComputeNewClockDelta` (`:914-984`). A bad delta makes
  relayed timestamps wrong for *other* clients (interpolation jump), it does not
  move the sender.
- **J.** Some ACK handlers deliberately trust client data, further confirming the
  client-authoritative model: `HandleMoveSetCanFlyAckOpcode` copies the client's
  flags straight into `m_movementInfo` (`src/game/Handlers/MiscHandler.cpp:1628`);
  `HandleMoveRootAck`/`HandleMoveUnRootAck` accept the client position and call
  `UpdatePosition` (`MovementHandler.cpp:986-1060`); `HandleMoveHoverAck` /
  `HandleMoveWaterWalkAck` only read and discard (`:822-852`).

## 4. Hypotheses for "server moved/teleported me"

Ranked by likelihood × regression risk of touching it.

| # | Hypothesis | Likelihood | Invasiveness to "fix" |
|---|---|---|---|
| 1 | Under-map handler triggers on a legit Z (`NearTeleportTo` / void death) — §3.A | Med–High | Low (threshold/safe-spot tweak) |
| 2 | Server spline active (charge/knockback/leap) + packets dropped, then snap — §3.B | Med | Med–High (core spline path) |
| 3 | Forced state packets (root/unroot/can-fly/hover/water-walk) — §3.C | Med | High (would change CC semantics) |
| 4 | Flag-stripping divergence on aura edge cases — §3.D | Low–Med | Med (removal risks freezing other clients) |
| 5 | False speed-ACK kick — §3.E | Low | Low (correct instead of kick) |
| 6 | Ping-burst kick — §3.F | Low | None (config only) |
| 7 | Clock-delta timestamp jitter for observers — §3.H | Low | High (netcode) |

If "teleport" reports cluster around knockback, charge, disengage, or arena
gates/elevators, hypotheses 1–2 are the prime suspects.

## 5. Conservative options (documented, NOT applied)

The review's whole point is that changes here are high-risk, so these are
deliberately small and separable. Nothing below is implemented.

| Opt | Change | Risk | Note |
|---|---|---|---|
| C1 | Keep the anticheat hooks inert; document loudly that adding a module re-arms movement anti-cheat | none | Prevents the exact failure mode |
| C2 | Env-only: `Warden.Enabled=false` and `MaxOverspeedPings=0` for the invite-only realm | none | Config; no code. **Left as-is by decision** |
| C3 | `HandleForceSpeedChangeAck`: correct + log instead of kick in the "faster" branch | Low | Movement file, one branch |
| C4 | Raise/soften arena under-map threshold so legit low ground/elevator doesn't trigger | Low–Med | Needs repro before touching |
| C5 | Do **not** remove `ReadMovementInfo` stripping | — | Removing it risks freezing *other* clients |
| C6 | Do **not** touch the spline lock or clock delta without a repro | — | Core hot paths |

## 6. Instrumentation first (cheap → expensive)

Before any change, instrument to see which path actually fires in a real match:

1. Under-map: log Z, `GetMinHeight`, and which `HandlePlayerUnderMap` at
   `MovementHandler.cpp:607`.
2. Spline lock: log when `:404` drops a packet (opcode + whether a spline is
   active) — high volume, gate behind a debug flag.
3. Speed ACK: log server vs echoed speed at `:723`.
4. Flag stripping: already logs under `ACORE_DEBUG`; enable for a session.

## 7. Out of scope / not investigated

- Charge/leap path quality — see `CHARGE_PATHING_ISSUES.md`. That document
  covers the largest genuinely server-authored player movement (the server
  builds the charge path and launches a spline).
- vmaps / LOS (`IsWithinLOSInMap`), separate from movement authority.
- Client-side prediction / interpolation internals.
- ArenaSpectator position streaming (`ArenaSpectator.cpp`) — spectator-only,
  does not affect players.
- Whether Warden should be on (network/memory concern, not movement).

## 8. Tooling

- Verify compilation: `zig build ac` (never parse `compile_commands.json`).
- Config is env-only (`.env`, template `.env.example`); keys like
  `Warden.Enabled` / `MaxOverspeedPings` are read in
  `src/game/World/World.cpp:1081,1505`.

---

# Addendum: cross-check vs vmangos + smoothness plan

Cross-checked against `/home/dawid/code/arenacraft2-reference-cores/core`
(vmangos, vanilla client — movement model is the same; refs prefixed `vmangos:`
use its layout: `src/game/Objects/`, `src/game/Anticheat/MovementAnticheat/`).
vmangos is the best open-source reference for "smooth + validated" movement;
its philosophy is the mirror image of what §3 catalogued.

## 9. What vmangos does differently

### 9.1 Reject, don't teleport/kick

- Every movement packet is gated on a reject window:
  `packet.movementInfo.stime <= m_moveRejectTime` (`vmangos:src/game/Handlers/MovementHandler.cpp:295`).
  Each `MovementInfo` carries **two clocks**: `ctime` (client send time) and
  `stime` (server receive time).
- Per-packet tests (`MovementAnticheat::HandleFlagTests` /
  `HandlePositionTests`, `vmangos:src/game/Anticheat/MovementAnticheat/MovementAnticheat.cpp:750,626`)
  accumulate **typed** cheat flags (teleport, speedhack, wall-climb, multi-jump,
  fly-hack, fake transport, time desync, ...). No hook/module indirection.
- On a rejectable violation the packet is dropped and the server **pushes
  correct state back to the client**: `SendSpeedChangeToAll`
  (`MovementAnticheat.cpp:726`), `ResolvePendingMovementChanges(true, true)`
  (`:729`), `SendHeartBeat(true)` (`:733`) — then rejects packets until
  `now + 100 + min(1000, worldDiff + latency)` (`:742`). The client resyncs
  itself. **No snap, no teleport, no kick.**
- Contrast with this core: kick on echoed speed (`MovementHandler.cpp:732-737`),
  otherwise zero per-packet validation, plus teleport-or-death on under-map
  (§3.A). This core's inert hook family (§2) maps ~1:1 onto vmangos's
  `MovementAnticheat`, but with reject+resync semantics instead of kick.

### 9.2 Extrapolation-based validation (not binary distance checks)

- `Unit::ExtrapolateMovement` (`vmangos:src/game/Objects/Unit.cpp:2865`)
  simulates where the client should be from the last accepted `MovementInfo`:
  run/strafe/turn-circle kinematics and **jump ballistics** (tracks
  `jump.startClientTime` + `m_jumpInitialSpeed`, set on `MSG_MOVE_JUMP` /
  `MSG_MOVE_FALL_LAND`, `MovementHandler.cpp:328-331`). Server splines
  extrapolate exactly via `movespline->ComputePositionAfterTime`.
- Speedhack check = integrated real distance vs extrapolated-allowed × 1.1,
  accumulated into `m_overspeedDistance` (`MovementAnticheat.cpp:1276-1332`) —
  tolerant accumulation, not a single-shot kill. Teleport check = distance vs
  config value scaled by the mover's speed rate, with an elevator/transport
  whitelist (`:1503-1576`).
- Time desync is accumulated per packet against the dual clocks and the credit
  a laggy client can bank is capped (`:1121-1142`, `:1289-1291`). This is a
  more precise primitive than our single `_timeSyncClockDelta` rewrite.

### 9.3 Spline-done handshake — the fix pattern for §3.B

- When the server stops driving a player-moved unit, `Unit::StopMoving` launches
  a `SetStop()` spline which makes the **client** send `CMSG_MOVE_SPLINE_DONE`
  (`vmangos:src/game/Objects/Unit.cpp:9077-9101`). Until that arrives,
  `HasPendingSplineDone()` gates stale movement packets
  (`Unit.h:1350`, used throughout vmangos' `MovementHandler.cpp`).
- `HandleSplineDone` (`MovementAnticheat.cpp:871`) validates the reported end
  position against `movespline->FinalDestination()` (10 yd tolerance) and
  rejects duplicate spline ids. The spline loop is therefore **closed and
  validated** instead of AC's silent `rfinish()` drop followed by whatever the
  client sends next (§3.B rubber-band).
- **Arenacraft already has the wire support**: `CMSG_MOVE_SPLINE_DONE` (0x2C9)
  is registered (`src/game/Server/Protocol/Opcodes.h:746`, handler
  `Opcodes.cpp:1050`) but consumed taxi-only (`src/game/Handlers/TaxiHandler.cpp:207`).
- Research step before porting: confirm the 3.3.5 client emits spline-done after
  self-targeted splines (vmangos *forces* the emission via the `SetStop()`
  spline trick; verify the same trigger works on 3.3.5).

### 9.4 Explicit "launched" (server-launched mover) state

- `SetLaunched` / `OnKnockBack` mark the mover as server-launched; teleport and
  speedhack checks and jump counters skip while launched
  (`MovementAnticheat.cpp:1506,1280,704-706`), and fall handling knows the
  descent is expected rather than suspicious.
- Directly relevant to `CHARGE_PATHING_ISSUES.md` symptom 3 (fall on charge
  arrival): this core has no launched state, so `HandleFall`
  (`MovementHandler.cpp:513-515`) judges charge landings with no context.

## 10. Re-ratings / gaps of §1–8 w.r.t. the smoothness goal

| Item | Original rating | Re-rated | Why |
|---|---|---|---|
| H (clock-delta timestamp rewrite) | Low | **Med–High for observer smoothness** | Every relayed movement packet's time is rewritten (`MovementHandler.cpp:565-574`); a drifting delta = interpolation jitter for everyone *watching* the mover — spectators included. Fallback path `:568` also `LOG_INFO`s per bad packet (spam) and mixes `getMSTime()` (uptime base, wraps ~49.7d) into a client-clock field. |
| B (spline lock) | Med | Med, **fix now known** | Post-charge rubber-band confirmed as the mechanism; §9.3 is the proven fix pattern. |
| A (under-map) | Med–High | unchanged | Top suspect; fix in Phase 1 with a sustained-trigger guard. |
| Pets/creatures | not covered | **gap** | Arena is full of server-driven movers (hunter/warlock/mage pets, fear/confuse). "All circumstances" includes them; separate work item. |

## 11. Plan: movement smoothness under all circumstances

**Design principle (arenacraft decision): err client-trusting, not
server-authoritative.** This is an invite-only arena realm; we optimize for
snappiness and simple logic. Locomotion stays fully client-authoritative, a
client that moves *unusually* is never corrected or resynced — only moved-into-
an-impossible-state clients get anything at all, and even then it's drop +
optional resync, never kick/teleport. Practical consequence: port vmangos's
*philosophy* (reject-don't-kick, resync-don't-teleport, closed spline loop) but
deliberately **not** its full per-packet extrapolation anticheat (§9.1–9.2) —
that layer trades our snappiness/simplicity goal for cheat detection we don't
need at this trust level. Phase 3 shrinks accordingly.

Direction (vmangos-derived): keep client authority for locomotion, close the
spline loop, fix observer-side time handling, and remove the few remaining
server-side correction paths.

**Phase 0 — instrument (extends §6, zero risk, first PR).** Counters/logs for:
under-map trigger (Z vs `GetMinHeight` + action taken), spline-lock drops
(opcode, spline active, gap since last accepted packet), speed-ACK
corrections/kicks, clock-delta changes >25 ms, flag-strip hits (env-gated, not
`ACORE_DEBUG`-only). One arena session of telemetry decides what's real.

**Phase 1 — low-risk correctness (each shippable solo).**
1. Speed-ACK "faster" branch → correct + re-send speed instead of kick
   (`MovementHandler.cpp:732-737`); vmangos pattern §9.1.
2. Clock-delta hygiene: rate-limit/demote the fallback log; clamp delta jumps;
   note the `getMSTime()` fallback base inconsistency.
3. Under-map (§3.A): require a sustained/multi-condition trigger (vmap-backed Z
   check or epsilon + N consecutive packets) so one jittery packet on legit low
   geometry can't teleport an arena player.

**Phase 2 — spline-done handshake (§9.3, the big smoothness win).** Validate
end position vs spline destination on `CMSG_MOVE_SPLINE_DONE`, clear the spline
lock there, adopt the validated position; add a launched state (§9.4) so
`HandleFall` handles charge landings. Also feeds `CHARGE_PATHING_ISSUES.md`
symptoms 2–3.

**Phase 3 — trim the remaining server-authority paths (client-trusting pass).**
Per the design principle above, this is mostly *removal*, not addition:
1. Speed-ACK: adopt the echoed speed instead of correcting/kicking
   (`MovementHandler.cpp:723-738`) — the server already owns every speed change
   it initiates; a mismatched echo costs nothing to accept.
2. Under-map (§3.A): keep, but sustained-trigger only (see Phase 1.3).
3. Spline lock: replace silent drop with the §9.3 handshake (Phase 2).
4. Flag stripping (§3.D): keep only the strips that prevent *other* clients
   freezing (ROOT, SPLINE_ENABLED); drop the rest so server view == client view.
5. No new validation layer: the inert anticheat hooks (§2) stay inert or get
   deleted; revisit only if actual cheating shows up in telemetry.

Open questions: (a) scope — players only or pets/creatures too; (b) how far to
push the client-trusting pass (adopt echoed speed blindly vs. log-and-adopt);
(c) confirm Phase 0 as the first PR.
