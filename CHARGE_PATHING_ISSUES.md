# Charge / pathfinding-spell issues (investigation notes)

Status: **unconfirmed hypothesis scan** — nothing instrumented or changed yet.
Audience: future contributor / AI picking this up.

## Reported symptoms

Path-finding spells (warrior Charge/Intercept, feral charge, any
`SPELL_EFFECT_CHARGE`) misbehave:

1. Sometimes **too restrictive** — the spell fails when a normal path clearly exists.
2. Sometimes takes a **straight path through air/terrain** where a normal path exists.
3. Sometimes the player **falls** on arrival.

These are likely 2–3 separate root causes, not one.

## How a charge is built (code flow)

1. **Cast validation / pre-generated path** — `Spell::CheckCast`, case
   `SPELL_EFFECT_CHARGE`:
   - `src/game/Spells/Spell.cpp:6415` gate: `DisableMgr::IsPathfindingEnabled(...) && NeedsExplicitUnitTarget()`.
   - `Spell.cpp:6429` creates `m_preGeneratedPath = std::make_unique<PathGenerator>(m_caster)`.
   - `Spell.cpp:6430` `m_preGeneratedPath->SetPathLengthLimit(range)` where
     `range = GetMaxRange(true, m_caster, this) * 1.5f + objSize` (`Spell.cpp:6426`).
   - `Spell.cpp:6435` reject if `PATHFIND_SHORT`.
   - `Spell.cpp:6436-6438` reject if `!result || PATHFIND_NOPATH || PATHFIND_INCOMPLETE`.
   - `Spell.cpp:6439` reject if `IsInvalidDestinationZ(target)`.
   - `Spell.cpp:6442-6443` `ShortenPathUntilDist(target, objSize)`.
2. **Effect** — `Spell::EffectCharge` (`src/game/Spells/SpellEffects.cpp:5016`):
   - `SpellEffects.cpp:5037` speed, `SpellEffects.cpp:5043` fallback straight-line
     `GetFirstCollisionPosition` when there is no pre-path, `SpellEffects.cpp:5048`
     uses the pre-generated path (`MoveCharge(PathGenerator const&, ...)`).
3. **Movement** — `MotionMaster::MoveCharge`:
   - Overload at `src/game/Movement/MotionMaster.cpp:693` (point).
   - Overload at `MotionMaster.cpp:724-737` (pre-path) **launches a raw spline**
     via `init.MovebyPath(path.GetPath())` — no facing, no re-path, no fall handling.
   - `src/game/Movement/MovementGenerators/PointMovementGenerator.cpp:49-50`
     uses `m_precomputedPath` when `size() > 2`; `:85-93` launches it.
4. **Path construction** — `src/game/Movement/MovementGenerators/PathGenerator.cpp`.

## Key findings (with exact references)

### F1 — `PathGenerator` silently returns navigation-free shortcuts

Whenever the navmesh is unusable, `PathGenerator` builds a 2-point straight
line and labels it as a **usable** path:

- No navmesh / `UNIT_STATE_IGNORE_PATHFINDING` / `!HaveTile(start)` /
  `!HaveTile(dest)`: `PathGenerator.cpp:74-81` → `BuildShortcut()` then
  `_type = PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH`.
- Start or end polygon not found: `PathGenerator.cpp:175-198`:
  ```cpp
  Creature const* creature = _source->ToCreature();
  ...
  bool canSwim = creature ? creature->CanSwim() : true;
  bool path    = creature ? creature->CanFly() : true;   // <-- player => ALWAYS true
  bool waterPath = IsWaterPath(_pathPoints);
  if (path || (waterPath && canSwim))
  {
    _type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
    return;
  }
  ```
  For a **player**, `creature == nullptr`, so `path` is always `true`. Every
  time the start/end poly is invalid (missing tile, target elevated / off
  navmesh, coarse mesh), the player gets a straight shortcut.
- Start/end far from poly (`distToStartPoly/EndPoly > 7.0f`):
  `PathGenerator.cpp:202-259`.

The charge validation at `Spell.cpp:6435-6439` only rejects
`PATHFIND_SHORT | PATHFIND_NOPATH | PATHFIND_INCOMPLETE`. It **never rejects
`PATHFIND_NOT_USING_PATH`**, so shortcuts pass and are flown as a straight
line → symptom 2, and often symptom 3 on landing.

### F2 — Point-path limit is derived from spell range, causing false `PATHFIND_SHORT`

`SetPathLengthLimit(distance)` computes
`_pointPathLimit = min(distance / SMOOTH_PATH_STEP_SIZE, MAX_POINT_PATH_LENGTH)`
(`PathGenerator.h:88-91`, `SMOOTH_PATH_STEP_SIZE = 4.0f` at `PathGenerator.h:39`).

`BuildPointPath` treats hitting that point count as failure:
`PathGenerator.cpp:569-574` → `BuildShortcut(); _type |= PATHFIND_SHORT;`

So a short-range charge (range*1.5 / 4 can be ~3 smooth points) fails as
`SPELL_FAILED_NOPATH` even when the destination is trivially reachable. This
is the most likely cause of symptom 1.

### F3 — Raycast "first try" comment is dead code

`Spell.cpp:6432-6433` comments *"first try with raycast, if it fails fall back
to normal path"*, but `SetUseRaycast(true)` is never called for charges and
there is no fallback logic. `SetUseRaycast` (`PathGenerator.h:92`) and the
raycast branch (`PathGenerator.cpp:405-466`) exist but are only used by other
call sites (chase/home), not charge.

### F4 — `IsInvalidDestinationZ` is one-sided and loose

`PathGenerator.cpp:1088-1091`:
```cpp
return (target->GetPositionZ() - GetActualEndPosition().z) > 5.0f;
```
Only rejects a target **more than 5y above** the path end. It does not reject a
path end far *below* the target, nor validate that the end is on the target's
footing. Contributes to symptom 3.

### F5 — End-point trim + Z normalization

`ShortenPathUntilDist` (`PathGenerator.cpp:1022-1086`) walks the smooth path
backwards toward the target and interpolates the final point; `NormalizePath`
(`PathGenerator.cpp:606-612`) then runs `WorldObject::UpdateAllowedPositionZ`
(`src/game/Entities/Object/Object.cpp:1549`) per point. On slopes/ledges/stairs
this can leave the final charge point clamped to ground/water height that does
not match the target's actual footing → drop on arrival. The spline is launched
verbatim at `MotionMaster.cpp:729-736` with no ground re-check at finalize
(`PointMovementGenerator.cpp:157-175`).

### F6 — Navmesh is on by default, but degrade paths exist

- `MoveMaps.Enable` defaults to **true**: `src/game/World/World.cpp:1543`
  (`CONFIG_ENABLE_MMAPS`).
- `DisableMgr::IsPathfindingEnabled` (`src/game/Conditions/DisableMgr.cpp:412-419`)
  also returns false for `MMapFactory::forbiddenMaps`.
- `forbiddenMaps` currently `{616 EoE, 649 ToC25, 650 ToC5}`:
  `src/common/Collision/Management/MMapFactory.cpp:37-46`. None are arenas.
- If a tile failed to build, `HaveTile` returns false (`PathGenerator.cpp:697-711`)
  → F1 shortcut.

## Hypothesis table

| # | Hypothesis | Prob. it fixes "seamless" | Difficulty to implement & test | Invasiveness (regression / crash risk) |
|---|---|---|---|---|
| 1 | Charge accepts `PATHFIND_NOT_USING_PATH` shortcut; player branch forces `path = true`. Fix validation + player default. `PathGenerator.cpp:74-81,175-198`; `Spell.cpp:6435-6439` | **High** for air/fall | Low | Low–Med (charges fail more until follow-up; no crash) |
| 2 | `SetPathLengthLimit(range)` → false `PATHFIND_SHORT`. `PathGenerator.h:88-91`; `PathGenerator.cpp:569-574`; `Spell.cpp:6430,6435` | **High** for too-restrictive | Low | Low (more permissive) |
| 3 | `IsInvalidDestinationZ` one-sided/loose. `PathGenerator.cpp:1088-1091` | Med | Low | Low |
| 4 | Raycast fallback never enabled (dead comment). `Spell.cpp:6429-6433`; `PathGenerator.cpp:92,405-466` | Med | Low–Med | Low–Med |
| 5 | End-point trim + Z normalization leaves bad footing → fall. `PathGenerator.cpp:1022-1086,606-612`; `MotionMaster.cpp:724-737` | Med | Med | Low–Med |
| 6 | Far-from-poly (>7y) shortcut for elevated/rooftop targets. `PathGenerator.cpp:202-259` | Med | Low–Med | Low–Med |
| 7 | Missing/failed mmap tile, or pathfinding disabled/forbidden map → shortcut. `PathGenerator.cpp:74-81,697-711`; `DisableMgr.cpp:412-419`; `MMapFactory.cpp:37-46` | Low (arenas) / Med open world | Low (inspect) | Low |
| 8 | Navmesh agent params (radius .53, height/climb 1.6) → hugs ledges / cuts corners. `src/tools/mmaps_generator/MapBuilder.cpp:1058-1073` | Med | High (edit + rebuild all mmaps, hours) | Med–High (connectivity regressions) |
| 9 | "Max fidelity" extraction: default already `bigBaseUnit=false`, `maxAngle=60`; physical agent size unchanged by `bigBaseUnit`. `src/tools/mmaps_generator/PathGenerator.cpp:235`; `MapBuilder.h:72-90`; `scripts/src/extract_assets.ts:60` | Low–Med | High (full re-extract; real gains need code) | Med (re-extract can regress, e.g. map 562 rope special-case at `MapBuilder.cpp:1077-1080`) |
| 10 | No off-mesh/jump links baked for arenas (`offMeshInputPath` never supplied). | Low–Med | High | Med |

## Suggested first steps (cheap → expensive)

1. **Instrument before changing.** Add a temporary log of `GetPathType()` and
   `_source->IsPlayer()` at `Spell.cpp:6435` and in `EffectCharge`
   (`SpellEffects.cpp:5037`). Reproduce one arena. This immediately separates
   F1 (`NOT_USING_PATH` shortcut) from F2 (`SHORT`).
2. Fix **F2** first (lowest risk): stop using spell range as the point-path
   limit for charges, or recompute the limit from the actual path length.
3. Fix **F1**: in the charge path validation, reject `PATHFIND_NOT_USING_PATH`
   (or force a real path), and fix the player default (`path = true` when
   `creature == nullptr`). Consider `IsFalling()`/`CanFly()` semantics for
   players explicitly.
4. Tighten **F3/F4** destination checks.
5. Only then consider generator changes (F8–F10); a re-extract is expensive and
   can regress working maps. Use `cs_mmaps` and the `mmap_events` metric
   (`PathGenerator.cpp:62`) for observation.

## Useful commands / tooling

- `zig build ac` to verify a change compiles. Do not parse `compile_commands.json`.
- Re-extract with mmaps: `bun scripts/extract_assets.ts --with-mmaps`.
- Generator knobs actually supported: `--maxAngle` (45–60), `--bigBaseUnit`,
  `--skipLiquid`, `--skipContinents`, `--skipJunkMaps`, `--skipBattlegrounds`,
  `--tile`, `--threads`, `--file` (`src/tools/mmaps_generator/PathGenerator.cpp:62-235`).
- Note `extract_assets.ts` passes **no** generator args (`extract_assets.ts:60`),
  so defaults apply.

## Out of scope / not investigated

- vmaps / LOS (`IsWithinLOSInMap`, `Spell.cpp:6425`) separate from pathing.
- Client-side anticipation of charge movement.
- Anticheat interaction (`sScriptMgr->AnticheatSetUnderACKmount` in
  `EffectCharge`, `SpellEffects.cpp:5051`).
