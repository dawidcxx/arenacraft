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
- `scripts/deploy` build the arm64 image and ship it to `dawid@hetznerbox`

## Deploy

The server is a small arm host, so the image is built elsewhere and shipped as
a single stream to the target's local container engine - no registry involved:

```bash
scripts/deploy                 # build linux/arm64, stream to dawid@hetznerbox
scripts/deploy --no-ship       # build + verify arch only
scripts/deploy --skip-build    # ship the image already on this machine
scripts/deploy --up --remote-dir ~/arenacraft   # also (re)start the stack there
```

On the server, after a deploy, `scripts/restart_world` recreates only the
`world` container from the freshly loaded image (`docker compose up -d --no-deps
world`), leaving db/valkey/auth untouched.

Works with **podman or docker** (auto-detected; override with `--engine`). The
target runs podman, so the remote side defaults to `--remote-engine podman`.
Building on a native arm64 machine (Apple Silicon) is direct and fast; a
cross-arch build on x86 Linux uses binfmt/qemu emulation, slow the first time
but cached.

Only the final multi-stage runtime image is transferred; zig's caches live in
`--mount=type=cache` mounts and never ship. The image lands as `arenacraft:local`
(the tag `docker-compose.yml` refers to), so on the target: `podman compose up -d
--no-build`.

On NixOS a binfmt entry without the `F` flag cannot see its interpreter inside a
container - the script detects this and bind-mounts `/run/binfmt` and `/nix/store`
into a podman build automatically.

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
