# Arenacraft

Custom WoW 3.3.5a arena core

## Setup

Needs `nix`, `podman` (or docker), and a 3.3.5a client.

1. `nix develop`
2. `podman compose up -d db valkey`
3. `cp .env.example .env` (db credentials, realm id, ...)
4. `./scripts/seed_db mysql://root:acore@localhost:3306` — one-time base seed
5. `zig build -Doptimize=ReleaseFast ac`
6. `scripts/extract_assets --client-dir <client>` writes `data/{dbc,maps,vmaps}`;
   add `--with-mmaps` to also build mmaps (slow, needs the ReleaseFast build
   because the extractors abort in debug)
7. `./zig-out/bin/ac db-update` applies pending `data/sql/updates`
8. `./zig-out/bin/ac authserver` or `./zig-out/bin/ac worldserver`

`.env` is loaded automatically; set `AC_DATA_DIR` if `data/` isn't next to the binary.

## Commands

- `zig build ac` debug build
- `zig build check-ac` compile everything without emitting an executable (PR CI)
- `zig build run-ac -- db-update` apply pending database updates
- `zig build run-ac -- worldserver` build and run
- `zig build test` unit tests (needs `nix develop`)
- `zig build test-build` dep/link smoke test
- CI: `.github/workflows/check.yml` runs `zig build check-ac` on every pull
  request; `.github/workflows/image.yml` publishes the amd64 image on pushes to
  `local`
- `scripts/deploy` build the amd64 image and ship it to the target
- `scripts/restart_world` restart the world service on the target and prune old images

## CI image

Pushes to `local` build and publish:

```text
ghcr.io/dawidcxx/arenacraft:latest
```

Only the current `latest` version is retained; older untagged GHCR versions are
pruned after each publish. The package must be public for unauthenticated pulls,
and pruning requires the repo to have Admin access to the package. PRs use the
Nix CI shell (`nix develop .#ci`) to run the compile-only check and never build
or push an image.

## Deploy

The server is an x86 host, so the image is built elsewhere and shipped as a
single stream to the target's local container engine - no registry involved:

```bash
scripts/deploy          # build linux/amd64 and stream the image to the target
podman run --rm --network <net> --env-file <env> arenacraft:local ac db-update
scripts/restart_world   # restart the world service there + prune old images
```

The runtime image carries only `data/sql/updates`; base seeding stays a
one-time development operation. Deployments without a separate migration step
can set `AC_DB_UPDATE_ON_STARTUP=1` so `worldserver` applies pending updates
before opening its DB pools. An empty `updates` table is refused instead of
replaying history; a schema-only restore must include its update tracking.

`scripts/deploy` only builds and ships the image (as `arenacraft:local`); the
running systemd-managed containers pick it up on their next restart, which
`scripts/restart_world` does (and then prunes the images left behind by repeated
deploys). Both scripts ssh to `arenacraft@netcupbox`; set `ARENACRAFT_HOST` to
override the target.

`deploy` auto-detects **podman or docker** locally and loads into podman on the
target. Building on a native amd64 machine is direct and fast; a cross-arch
build uses binfmt/qemu emulation, slow the first time but cached.

Only the final multi-stage runtime image is transferred; zig's caches live in
`--mount=type=cache` mounts and never ship. The image lands as `arenacraft:local`.

The source revision is baked into the binary at build time: `ac worldserver
--version`, the startup banner and the in-game `.server info` command all show
the commit. The image also carries `/app/server-info` with the full commit,
branch and commit date for quick deployment checks.

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
