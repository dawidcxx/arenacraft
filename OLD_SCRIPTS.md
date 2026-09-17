# OLD_SCRIPTS.md

The old `scripts/` bun tooling was removed with the cmake build. Reference for
the eventual clean rewrite, which should only cover **db management
(migrations/backup)** and **client data extraction**. Everything else
(build/install/service management) is intentionally dropped - service
management is obsolete now that the core ships as a single `ac` binary and
configs resolve next to the executable.

> Update: the rewrite has started - `scripts/db_sync` (forward-only sql sync
> into the target databases, `scripts/db_sync --help`) and a skeleton
> `scripts/extract_assets` now live in `scripts/src/`, invoked via the
> `scripts/<tool>` symlinks. The patterns below are still the reference for
> what remains to be ported.

## What existed

- `bin/install` (`src/install.ts`) - cmake configure + build + install to
  `~/.local/arenacraft` (bin/, etc/), plus systemd user units
  (`arenacraft-world` / `arenacraft-auth`) pointing at the old standalone
  `worldserver` / `authserver` binaries. Junk today: `zig build ac` produces
  one binary that routes subcommands (`ac worldserver`, `ac authserver`,
  `ac map_extractor`, ...).
- `bin/clean` (`src/clean.ts`) - `rm -rf ./build`.
- `bin/healthcheck` (`src/healthcheck.ts`) - checked `which cmake/worldserver/...`
  systemd units and mysql/redis/auth port liveness. Junk for the same reason.
- `bin/link` (`src/link.ts`) - symlinked `worldserver.conf` / `authserver.conf` /
  `dbimport.conf` from `~/.local/arenacraft/etc` into the repo root for dev
  runs. `dbimport.conf` is gone (dbimport never ported); config discovery is
  exe-relative.
- `bin/link-script` (`src/link-script.ts`) - dev helper to symlink a
  `scripts/src/*.ts` into `scripts/bin/`. Irrelevant once scripts are rewritten.
- `bin/snapshot-db` (`src/snapshot-db.ts`) - docker-based mysql backup:
  `docker run --rm -v mysql-data:/data -v ./backups:/backup alpine tar -czf
  /backup/mysql-snapshot-<timestamp>.tar.gz -C /data .`. KEEP pattern for the
  db tooling rewrite.
- `bin/extract-client` (`src/extract-client.ts`) - client data extraction,
  KEEP pattern for the rewrite:
  - prompts for game dir (default `~/.local/var/wow`) and dest dir
    (default `~/.local/arenacraft/data`)
  - required the old standalone binaries `map_extractor`, `vmap4_extractor`,
    `vmap4_assembler`, optional `mmaps_generator` (prompt; slowest step)
  - today: run them via `zig build ac`, then `./zig-out/bin/ac map_extractor`
    etc. against the client dir (see src/tools), output goes to the dest dir
- `bin/shared.ts` - `requireProjectDir` (ls src/apps + flake.nix),
  `requireProgram`, `formatElapsedTime`, `info`.

## Current build/dev flow (replacement)

- `nix develop` for the toolchain
- `zig build ac` (or `-Doptimize=ReleaseFast ac`)
- configs: copy `src/auth/authserver.conf.dist` / `src/worldserver/worldserver.conf.dist`
  next to the `ac` binary (`.dist` stripped)
