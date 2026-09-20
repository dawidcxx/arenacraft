# HOST_DEPLOY — continuation notes

> **TEMPORARY.** Delete this file once the deploy pipeline is verified green
> (emulated aarch64 build + cache behaviour + ship/load). It exists only to hand
> off the long-running build work to the next session.

State as of: 2026-09-20 (host `geekombox`, x86_64, NixOS 26.05pre).

## UPDATE (2026-09-20, later)

- **Emulated cross-build on x86 was abandoned** in favour of building natively
  on an Apple Silicon mac, where it builds and caches properly at native speed.
- `scripts/deploy` is now **engine-agnostic**: auto-detects `podman`/`docker`
  (`--engine`), and ships to the target's engine (`--remote-engine`, default
  `podman`). Docker uses plain `docker save`; podman uses `--format docker-archive`.
  The podman-only binfmt mounts are skipped for docker (buildx has its own qemu).
- **Caching was proven healthy on x86/podman** with a minimal probe:
  - a no-rebuild re-run reported `Using cache …` on the `RUN --mount=type=cache`
    step (layer cache works);
  - changing an `ARG` invalidated the layer but the cache-mount contents persisted
    (`run T=1` still present on the next run).
  So the slow emulated runs were simply because no build ever **completed** —
  the layer cache only helps after one successful build.
- On x86 the remaining options (not needed now) were: cross-compile to aarch64
  instead of emulating, or just accept one long first build; the `.zig-cache`
  (5.6 GB at `/var/tmp/buildah-cache-1000/ea562abaa939e7f0`) persists across runs.

The rest of this file documents the x86/NixOS path and is kept for reference.

## Goal

`scripts/deploy` (Bun) builds the core image for the **arm server** on a fast
x86 host under qemu emulation, then loads it into the **target's local podman**
(no registry). Default tag `arenacraft:local`, default target `dawid@hetznerbox`.

```
scripts/deploy                       # build + ship (zero args, the normal call)
scripts/deploy --no-ship             # build + verify arch only
scripts/deploy --skip-build          # ship the image already here
scripts/deploy --up --remote-dir DIR # also `podman compose up -d --no-build`
scripts/deploy --dry-run             # print commands only
```

Ship step: `podman save --format docker-archive arenacraft:local | gzip -1 | ssh <target> 'gunzip | podman load'`.
Only the final multi-stage runtime image is transferred; zig/build caches stay local.

## Current status: BUILD NOT YET COMPLETED

The only thing left is to **finish one full emulated build**. There have been no
errors in the build — it reaches `STEP 12/12 ... zig build -Doptimize=ReleaseFast ac`
and compiles for a long time under qemu. Do not assume breakage from duration.

- No `arenacraft:local` image exists yet (`podman image inspect arenacraft:local` errors).
- The first full emulated build is genuinely long (tens of minutes; the user
  expected ~45 min and has seen it need a second "warmup" run).
- Cache reuse across runs **is** working (see below), so a resumed run is faster.

### Important: the build may be an orphan

Interrupting the tool call does **not** always kill `podman build`; it can keep
running detached and still produce the image. Before starting another build:

```bash
pgrep -af 'podman build --platform linux/arm64'   # is one already running?
tail -f /tmp/build1.log                           # live output of the orphan
```

If one is running, **wait for it** rather than launching a competing build.
Recent logs: `/tmp/build1.log` (current/recent), `/tmp/deploy-build.log` (first 25 min run).

## What is already in place

| Path | Change |
|---|---|
| `scripts/src/deploy.ts` (+ symlink `scripts/deploy`) | new Bun script; uses `import { $ } from "bun"` for build/ship/capture |
| `Dockerfile` | base images fully-qualified `docker.io/library/ubuntu`; `DEBIAN_FRONTEND=noninteractive` (both stages) |
| `.dockerignore` | also excludes `/ac` (239 MB debug binary), `/apps`, `/scripts`, `/ai-doc` |
| `docker-compose.yml` | `arenacraft-core:latest` → `arenacraft:local` (auth + world) |
| `README.md` | "Deploy" section |
| `flake.nix` | devshell adds `skopeo`, `zstd`, `jq` |

Script details:
- `qemuMounts()` auto-detects a NixOS binfmt entry **without the `F` flag** and
  adds `-v /run/binfmt:/run/binfmt:ro -v /nix/store:/nix/store:ro` to the build.
- Options: `--target`, `--image`, `--platform`, `--skip-build`, `--no-ship`,
  `--up`, `--remote-dir`, `--dry-run`.
- Verify with: `cd scripts && bun run typecheck` (tsc, clean).

## Host quirk you must not forget (the reason for qemuMounts)

This host's binfmt entry is:

```
:aarch64-linux:M:...:/run/binfmt/aarch64-linux:P          # flags: P, no F
```

`/run/binfmt/aarch64-linux` is a **static shim** that execs
`/nix/store/...-qemu-user-11.1.0/bin/qemu-aarch64` (dynamically linked). Without
`F` the kernel resolves the interpreter inside the container's mount ns, and the
store path for the real qemu isn't there → `exec container process ... No such
file or directory`. Bind-mounting `/run/binfmt` + `/nix/store` into the build
fixes it. Confirmed:

```bash
# fails without the mounts, works with them:
podman run --rm --platform linux/arm64 \
  -v /run/binfmt:/run/binfmt:ro -v /nix/store:/nix/store:ro \
  docker.io/library/ubuntu:22.04 uname -m     # -> aarch64
```

Do **not** propose magic-byte `boot.binfmt.registrations` config to the user;
they consider that non-stock. Keep `boot.binfmt.emulatedSystems`.
The user's `ubuntu:22.04` tag was left pointing at amd64 (host-native).

## Cache findings (already confirmed)

Buildah cache root: `/var/tmp/buildah-cache-1000/` (persistent across runs, not `/tmp`).
Cache mounts in the Dockerfile: `arenacraft-zig-cache` (`/app/.zig-cache`),
`arenacraft-zig-pkg` (`/app/zig-pkg`), `arenacraft-zig-global` (`/root/.cache/zig`).

- The active run is reusing the **5.5 GB** `.zig-cache` dir `ea562abaa939e7f0`
  (created 2025-09-19) — i.e. caches persist and are reused across invocations.
- There is an older duplicate set from an earlier Dockerfile/config generation
  (`7c006f6984cff557` 9.2 GB, `88706e5c70ad7fce` 594 MB) wasting ~10 GB. Optional
  cleanup: `rm -rf` those two dirs (verify no build is running first).
- Open item: the cache `id`s are not architecture-scoped, so amd64 and arm64
  builds could share a cache dir. Zig's cache is content-addressed (target is
  part of the key) so it should not *corrupt* anything, but the user suspects
  "poisoning". Test whether `${TARGETARCH}` expands inside
  `--mount=type=cache,id=...`; if yes, switch ids to `arenacraft-zig-cache-${TARGETARCH}`
  (note: this starts a fresh cache → one more full build). Only do this after the
  green build if desired.

## Test plan

Run on the server-grade machine. Loop until success is fine — the caches make
each retry cheaper. Do not ssh to the real target until the user allows it.

### 0. Sanity — emulation works
```bash
podman run --rm --platform linux/arm64 \
  -v /run/binfmt:/run/binfmt:ro -v /nix/store:/nix/store:ro \
  docker.io/library/ubuntu:22.04 uname -m
```
Expect `aarch64`. (Without the two `-v` mounts it fails with `No such file or directory`.)

### 1. Script plumbing
```bash
cd scripts && bun run typecheck          # clean
./deploy --dry-run                       # prints build + inspect + save|ssh|load
```
Expect the build line to include the two `-v` mounts (binfmt has no `F`).

### 2. Full build to completion
```bash
# wait if one is already running (see "orphan" note), else:
./scripts/deploy --no-ship
# success looks like:
#   [2/2] STEP ... runtime stage
#   COMMIT arenacraft:local
#   Successfully tagged localhost/arenacraft:local
#   built arenacraft:local (arm64)
podman image inspect --format '{{.Architecture}} {{.Size}}' arenacraft:local
```
Expect `arm64` and a thin size (runtime stage only, ~tens of MB of libs + `ac`).

### 3. Cache: no-op rebuild is (near) instant
```bash
time ./scripts/deploy --no-ship
```
Expect: build layers served from cache, **no long `zig build`** (seconds to ~1
min). Look for `Using cache` on the `RUN zig build` step. This is the key proof
the layering works.

### 4. Cache: trivial source change is incremental, not 45 min
```bash
# make a content change (mtime-only `touch` will NOT invalidate COPY):
#   add and then remove a comment line in, e.g.,
#   src/game/Arenacraft/npcs/ItemVendorItems.cpp
time ./scripts/deploy --no-ship
# revert the change afterwards
```
Expect: `COPY src` invalidated, `zig build` re-runs, but only the changed TU +
link — minutes, **not** the full ~45 min. If it recompiles everything, the
`.zig-cache` mount is not being reused → investigate before proceeding.

### 5. Ship locally (no real remote) — optional
Stand up a throwaway ssh target (e.g. `ssh localhost`) or use
`--target <user>@localhost` and confirm:
```bash
./scripts/deploy --skip-build --target <user>@localhost
# then on the "target":
podman image inspect --format '{{.Architecture}}' arenacraft:local   # arm64
```
This exercises `save | gzip | ssh | gunzip | load` without touching the user's box.

### 6. Real ship / restart — **only with user's go-ahead**
```bash
./scripts/deploy                                  # build + ship to dawid@hetznerbox
./scripts/deploy --up --remote-dir ~/arenacraft   # + compose up -d --no-build
```

### 7. When all green
- Remove `HOST_DEPLOY.md`.
- Optionally clean the stale ~10 GB cache dirs listed above.
- Decide on the arch-scoped cache-id change (would need one more full build).

## Pitfalls / gotchas

- **Duration is not failure.** Under qemu the `zig build` step runs for a long time;
  ~9 concurrent `qemu-aarch64` processes is normal (zig parallelises).
- **Short image names don't resolve** on this host (`/etc/containers/registries.conf`
  has no `unqualified-search-registries`) — that's why the Dockerfile base images
  are fully qualified. Use `docker.io/library/...` in any ad-hoc `podman run`.
- `localhost/` prefix: podman stores the tag as `localhost/arenacraft:local`;
  compose's `image: arenacraft:local` resolves to it.
- The build context is huge without `.dockerignore` (the root `ac` is 239 MB);
  keep `/ac` excluded.
- `--dry-run` uses lazy `$` correctly: commands are only constructed when not
  dry-run, so nothing executes.
- Cleanup already done: probe images (`armtest`, `acprobe`) removed; `ubuntu:22.04`
  restored to amd64.

## Cheat sheet

```bash
# is a build running?
pgrep -af 'podman build --platform linux/arm64'
# follow it
tail -f /tmp/build1.log
# cache usage
du -sh /var/tmp/buildah-cache-1000/* 2>/dev/null
# typecheck the script
(cd scripts && bun run typecheck)
# binfmt entry (expect "flags: P", no F)
cat /proc/sys/fs/binfmt_misc/aarch64-linux
```
