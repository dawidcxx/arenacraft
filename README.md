# Arenacraft

Custom WoW 3.3.5a arena core

## Setup

Needs `nix`, `podman` (or docker), and a 3.3.5a client.

1. `nix develop`
2. `podman compose up -d db valkey`
3. `cp .env.example .env` (db credentials, realm id, ...)
4. `zig build -Doptimize=ReleaseFast ac`
5. `scripts/db_sync` populates the `acore_*` databases from `data/sql`
6. `scripts/extract_assets --client-dir <client>` writes `data/{dbc,maps,vmaps}`;
   add `--with-mmaps` to also build mmaps (slow, needs the ReleaseFast build
   because the extractors abort in debug)
7. `./zig-out/bin/ac authserver` or `./zig-out/bin/ac worldserver`

`.env` is loaded automatically; set `AC_DATA_DIR` if `data/` isn't next to the binary.

## Commands

- `zig build ac` debug build
- `zig build run-ac -- worldserver` build and run
- `zig build test` unit tests (needs `nix develop`)
- `zig build test-build` dep/link smoke test

## Modules

`ac` is the only binary; it routes subcommands (`worldserver`, `authserver`, the
extractors) to static libraries:

- `common`: utils, logging, crypto, network
- `database` -> `common`
- `shared` -> `database`
- `auth` -> `shared`
- `tools` -> `common`
- `game` -> `shared`; gameplay and scripts live together here (`src/game/Scripts/`)
- `worldserver` -> `game`

No separate scripts or module layer; custom gameplay is plain code in `src/game/`.
