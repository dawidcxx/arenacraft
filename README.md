# Arenacraft

Custom mmo core based off [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk)

# Module structure

The build is a layered set of static libraries (see `zig-build/Src.zig`), each
linking its parent and re-exposing its include paths:

```
ac (executable, src/ac/main.cpp)
├── worldserver ── game ── shared ── database ── common
├── auth ──────── shared ── database ── common
└── tools ─────── common
```

- `common` — root: utilities, logging, crypto, networking. No internal deps.
- `database` → common, `shared` → database, `auth` → shared, `tools` → common.
- `game` — all gameplay, **including scripts** (`src/game/Scripts/`). Scripts
  are plain game code compiled into the `game` library: no separate module, no
  dynamic loading. New script loaders are registered by hand in
  `src/game/Scripts/ScriptLoader.cpp`.
- `worldserver` → game.
- `ac` is the single unified executable; it links worldserver + auth + tools and
  routes subcommands (`ac worldserver`, `ac authserver`, ...).
- There are deliberately no `modules`/`scripts` build nodes; custom gameplay
  lives directly in `src/game/`.

Build the executable with `zig build ac`, or build + run it with
`zig build run-ac` (args are forwarded: `zig build run-ac -- worldserver`).

# Install

Dependencies: `nix` `podman` (or docker) `wow-3.3.5a-client-files`

1. setup dev env
- `nix develop`
2. runtime services (mysql for db_sync, valkey for worldserver)
- `podman compose up -d db valkey`
3. configure environment
- `cp .env.example .env` (database credentials, realm id, ...)
4. build core
- `zig build -Doptimize=ReleaseFast ac` (plain `zig build ac` for a debug build)
5. populate databases
- `scripts/db_sync` (creates the acore_* databases from `data/sql`, forward only, safe to re-run)
6. extract client assets
- `scripts/extract_assets --client-dir <path-to-client>` (writes `data/dbc`, `data/maps`, `data/vmaps`; `--with-mmaps` to also build mmaps, slow)
- use the ReleaseFast build, the extractors abort in debug (ubsan)
7. run the core
- `./zig-out/bin/ac authserver` / `./zig-out/bin/ac worldserver` (loads `./.env` automatically)
- set `AC_DATA_DIR=../../data` in `.env` (resolved relative to the binary; only `data/` next to it by default)
