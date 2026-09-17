# Arenacraft

Custom mmo core based off [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk)

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
