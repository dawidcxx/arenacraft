# AGENTS.md

Arenacraft: invite-only WoW 3.3.5 arena private server. C++23 core forked from
AzerothCore, now fully ours — we do NOT rebase against Trinity/AzerothCore
anymore. Modify the core directly; don't build plugin/module abstractions or
preserve upstream compatibility.

Gameplay loop: player logs in at level 80, gears up via vendors (gear templates
planned), plays arena. Custom gameplay is plain core code in `src/game/`, not a
script/plugin layer.

## Build & run

- `nix develop` (or direnv) for the toolchain: zig, clang-tools, bun, mysql84, ...
- `zig build ac` — debug. `-Doptimize=ReleaseFast ac` for actual play.
- `zig build test-build` — dep/link smoke test (the only "test" there is).
- `compile_commands.json` auto-regenerates after builds (clangd).
- Single unified binary `ac`; subcommands: `ac worldserver`, `ac authserver`,
  `ac map_extractor`, `ac vmap4_extractor`, `ac vmap4_assembler`, `ac mmaps_generator`.
- Services: `podman compose up -d db valkey` (mysql 8.0 + valkey).
- Config is env-only, no .conf files: `./.env` auto-loaded (template
  `.env.example`); `AC_DATA_DIR` resolves relative to the binary.

## Database

- `scripts/db_sync` (bun) — forward-only sync of `data/sql` into `acore_*` dbs.
  No rollbacks; the AC in-core updater was removed on purpose.
- Schema = squashed base in `data/sql/base/db_{auth,characters,world}`.
- New migration: add `data/sql/updates/db_<name>/YYYY_MM_DD_NN.sql`. Never edit
  an applied file — sha1-tracked, modified files get skipped.
- Idempotent SQL (REPLACE INTO etc.) may go in `data/sql/custom/` (gitignored).
- `data/dbc|maps|vmaps[,mmaps]` come from `scripts/extract_assets` — generated,
  never commit or hand-edit.

## Where gameplay code goes

- Write custom systems directly in the core (`src/game/`). For event hooks use
  the script registries in `src/game/Scripting/ScriptDefines/` (PlayerScript,
  AllCreatureScript, ArenaScript, ...).
- Custom player lifecycle (login/level-80 flow): write plain core code in
  `src/game/`.
- Spell fixes:
  - runtime DBC-level corrections → `src/game/Spells/SpellInfoCorrections.cpp`
    (`ApplySpellFix(...)`)
  - scripted behavior → `src/game/Scripts/Spells/spell_<class>.cpp` with
    `RegisterSpellScript`, bound via `spell_script_names` DB table
- Vendor items: `npc_vendor` rows.
- Scripts are plain game code, compiled into the `game` library (no separate
  module, no dynamic loading). The registry is hand-maintained — edit directly,
  no globs: `src/game/Scripts/ScriptLoader.cpp`.

## Conventions & traps

- clang-format touched files (LLVM base, Allman braces, 120 cols, 2-space —
  see `.clang-format`).
- Commits: lowercase, `arenacraft(scope): summary` / `feat:` / `fix:`.
- Repo was restructured from upstream: `src/server/game/...` → `src/game/...`.
  AC wiki docs and old diffs reference stale paths.
- `.env` holds secrets; never commit it.
