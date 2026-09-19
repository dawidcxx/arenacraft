# Code Review: zig build / Dockerfile de-schizo (staged worktree changes)

Scope: `Dockerfile`, `flake.nix`, `zig-build/Deps.zig`, `zig-build/Src.zig`,
`zig-build/cppkit-zig/CompileCommandsImpl.zig` (staged, uncommitted).

## Verdict

**Approve direction.** The change correctly identifies the root cause that forced
all the old workarounds: zig was silently falling back to its bundled libc, so
`/usr/include` (incl. the multiarch dir holding `opensslconf.h`) was invisible.
Installing `gcc` to enable zig's native libc detection fixes that root cause and
legitimately retires three separate hacks (openssl `.pc` stub with multiarch dir,
hiredis `.pc` stub, `PKG_CONFIG_ALLOW_SYSTEM_CFLAGS` / per-module
pkg-config include plumbing in Deps.zig). `CPATH` is the right primitive: it is
honored natively by zig's clang, searched after `-I` (vendored headers still win),
and both environments use narrow dirs (`/usr/include/mysql`, nix store paths) so
nothing shadows zig's bundled libc/libc++ headers.

Static checks performed (all clean):

- No stale references to removed helpers (`includeOpenSSL`, `includeHiredis`,
  `includeMysqlClient`, `includeZlib`, `includeBzip2`, `includeReadline`,
  `mysqlIncludeDirs`, `pkgConfigIncludeDirs`) anywhere in `zig-build/`.
- `deps/bzip2` is a clean upstream subset: 7 library files + headers, **no**
  `bzip2.c` / `bzip2recover.c`, so no stray `main()` symbols enter `libmpq.a`.
- `.dockerignore` does not exclude `deps/bzip2`; `COPY deps deps` carries it.
- `src/` never includes bzlib directly — libmpq is the only bzlib consumer, so
  static-linking bz2 into libmpq + dropping `libbz2-dev` (builder) and
  `libbz2-1.0` (runtime) is self-consistent. `libmysqlclient21` does not depend
  on libbz2.
- Include styles in `src/` match the CPATH plan: `<mysql.h>` (needs the mysql
  dir on the path — covered by CPATH in both envs), `<hiredis/hiredis.h>` and
  `<openssl/*.h>` (covered by `/usr/include` via libc detection in Docker, and
  by pkg-config `-I` flow / CPATH in nix).
- Nix nested mysql layout (`include/mysql/mysql/udf_registration_types.h`
  included as `"mysql/..."`) still resolves: CPATH points at `include/mysql`
  itself, which is both the dir holding `mysql.h` and the dir holding the nested
  `mysql/` component. `MYSQL_INCLUDE_DIR` removal is safe; `MYSQL_LIB_DIR` is
  still set in both envs (Dockerfile:62, flake.nix:57).
- nixpkgs `zig` resolves to `0.16.0`, matching Docker's pinned `ZIG_VERSION=0.16.0`.
- `pkg-config` correctly remains installed: `linkSystemLibrary` still uses it for
  `-l`/`-L`; only the cflags plumbing was removed. (Observed during testing: zig
  flows pkg-config cflags of linked system libs onto *final executables* — the old
  Deps.zig helpers were only ever needed for intermediate static libs.)

## Experiments run (all reverted afterwards; tree == staged state)

| Experiment | Result |
|---|---|
| `nix develop -c zig build test-build` on staged state | PASS |
| Inspected regenerated `compile_commands.json` | 607 entries; CPATH dirs (mysql, openssl, zlib, jemalloc, hiredis, readline, ncurses) mirrored as `-isystem` on every C/C++ entry — clangd will resolve the same headers the build does |
| Build with `CPATH`/`MYSQL_LIB_DIR` unset, fresh cache | FAILS as predicted: raw `'mysql.h' file not found` (zig-build/Test.cpp:24) with zero hint that env vars are missing — motivates finding #4 |
| Replaced vendored bz2 with system bzip2 (`linkSystemLibrary("bz2", .{})` + `pkgs.bzip2` in flake CPATH, 3 lines total) | PASS — proves vendoring was not the only option; see finding #2 |
| `zig build ac` (full debug build, catches `src/` include breakage) | PASS (exit 0) |

## Findings

| # | Severity | Location | Finding | Recommendation | Response (DeepSeek) |
|---|----------|----------|---------|----------------|---------------------|
| 1 | info | Dockerfile:23-36, Deps.zig:144 | Root-cause fix is correct and well-commented: gcc enables zig native libc detection, retiring the openssl multiarch stub, hiredis stub and pkg-config include plumbing. | None — keep. | Ack — this is the root-cause fix; kept. |
| 2 | medium | Deps.zig:397-408 | Vendoring bzip2 works (verified clean subset, no `main()`s) but is the more invasive of two viable fixes, and shifts bzip2 CVE management onto us (libmpq parses untrusted-ish archive files). A 3-line alternative was validated in the nix env: `mod.linkSystemLibrary("bz2", .{})` in `linkSystemLibraries` + `pkgs.bzip2` added to flake CPATH (mpq needs `bzlib.h`; Docker needs no CPATH entry since `/usr/include` comes from libc detection; Debian ships no `bzip2.pc` but zig falls back to `-lbz2` there). | Either is acceptable — make a conscious choice. Keep vendoring if Linux/Windows uniformity is the goal; otherwise swap to the 3-line system variant for distro CVE patches and a smaller build graph. Do not leave it un-decided. | Kept vendoring, decided consciously — rationale in Author response #2. |
| 3 | low | flake.nix:43,49 | Dead packages in the dev shell: `minizip` has no consumer anywhere in `src/` or `zig-build/` (pre-existing), and `bzip2` becomes dead if vendoring is kept (finding #2). | Remove `minizip`; remove `bzip2` only if vendoring is kept. | Done — dropped `minizip` + `bzip2` inputs and trimmed CPATH to the consumed deps. |
| 4 | low | zig-build/Deps.zig (init) / build.zig | The build now hard-couples to ambient env vars (`CPATH`, `MYSQL_LIB_DIR`) with no fail-fast. Verified failure mode: confusing `'mysql.h' file not found` deep inside a consumer TU. | Add an early check in `build()` that errors with an actionable message ("CPATH/MYSQL_LIB_DIR not set — run inside `nix develop` or the Dockerfile builder stage") when `environ_map` lacks them. Preserves the simplification, keeps failures humane. | Done — added to `AcGraph.build`; one caveat on the follow-on panic. |
| 5 | info | CompileCommandsImpl.zig:262-267, 365 | CPATH mirroring into `compile_commands.json` works (verified in output: mysql/openssl/zlib/... dirs present as `-isystem` on all 607 entries). Nit: CPATH is semantically `-I` (user dirs, searched before system paths) but is mirrored as `-isystem`; harmless here because no header names collide, but it is a semantic lie. | Optional: leave as is, or emit `-I` for exactness. No action required. | Done — emits `-I` now; verified 607/607 entries. |
| 6 | low | Dockerfile:17 vs flake.nix:32 | Docker pins `ZIG_VERSION=0.16.0`; nix gets zig from nixpkgs-unstable (currently also 0.16.0, verified). Nothing pins them to each other — a future nixpkgs bump would silently diverge the two environments (this whole fix relies on 0.16 libc-detection behavior). | Add a comment in both files cross-referencing each other, or pin `zig` in flake.nix inputs (e.g. `github:mlugg/zig/<hash>` / `nixpkgs#zig_0_16`) so they can only move together. | Partial — cross-comments both sides + configure-time warning; no version pin. |
| 7 | info | Deps.zig linkSystemLibraries | Zig's pkg-config flow observed on the failed link line: openssl/hiredis/zlib/readline/jemalloc `.pc` cflags already reach final exes, so the old per-module helpers were only needed for intermediate static libs — their removal is safe, and g3dlite/gsoap/libmpq zlib headers are covered by CPATH (nix) and `/usr/include` (Docker). | None — informational confirmation. | Ack. |
| 8 | info | Cache behavior | Cache invalidation remains correct: zig's manifest tracks headers by path+hash, so nix store path bumps or header content changes bust the cache despite CPATH being ambient. | None. | Ack. |

## Author response

### #2 bzip2 — deliberate choice: keep vendoring
- The bzip2 sources are **already in-tree** (`deps/bzip2`, upstream kept them for
  Windows). Compiling them adds no new third-party code; leaving them unbuilt
  just leaves dead source. So the CVE-tracking burden exists either way.
- One code path for Linux and Windows, deterministic, no `libbz2-dev`/`libbz2-1.0`
  in the images.
- The 3-line system variant (`linkSystemLibrary("bz2")`, no `.pc` → zig's
  `-lbz2` fallback) depends on implicit `LIBRARY_PATH`-style resolution on nix
  and re-adds bzip2 to both images. Given this whole change is about removing
  implicit magic, I kept the explicit vendored build.
- Verified end-to-end: `BZ2_bzDecompress` present in the produced binary, `ldd`
  shows no `libbz2`.

### #3 dead packages
Removed `minizip` and `bzip2` from `nativeBuildInputs`, and trimmed the CPATH
list to the consumed deps (`openssl hiredis zlib readline jemalloc` + mysql).
Confirmed the g3dlite `zip.h` includes are behind `#if _HAVE_ZIP`, which is never
defined, so `minizip` is genuinely unused. (`doctest`/`ncurses` remain as
pre-existing dev inputs, no longer mirrored into CPATH.)

### #4 fail-fast
Added to `AcGraph.build`: requires `CPATH` and `MYSQL_LIB_DIR`, logs
`missing build environment variable <X>; run inside `nix develop` or the
Dockerfile builder stage`, returns `error.MissingBuildEnvironment`.
Caveat: the pre-existing root `build.zig` catches every error with
`std.debug.panic`, so a panic trace follows the message. The actionable line is
printed first; happy to special-case this error to a clean exit if preferred.

### #5 CPATH flag semantics
`appendEnvIncludes` now emits `-I` (matching clang's CPATH semantics), appended
after module include dirs so vendored headers keep precedence. Output check:
607/607 entries resolve openssl via `-I`, 0 via `-isystem`.

### #6 zig version coupling
Added cross-referencing comments in `Dockerfile` and `flake.nix`, plus a
configure-time warning in `zig-build/build.zig` when zig is not 0.16.x. Did not
pin a hash/attribute — that's a maintainer call.

## Verification results

- [x] `zig build ac` through a fresh `nix develop` (uses the flake's exported
      `CPATH`) — exit 0.
- [x] Fail-fast: `env -u CPATH -u MYSQL_LIB_DIR zig build ac` prints the
      actionable message.
- [x] Docker `linux/amd64` build — exit 0; produced binary is byte-identical to
      the previous revision (deterministic).
- [ ] Docker `linux/arm64` — this host has no binfmt/qemu emulation, so it
      cannot run here. The path is arch-agnostic (`uname -m` drives the zig
      tarball and `MYSQL_LIB_DIR`; `gcc` on arm64 enables aarch64 host-libc
      detection).
- [x] Runtime smoke: `ac worldserver` reaches `Failed to connect to Redis`
      (i.e. mysqlclient/openssl/hiredis/jemalloc all load).
- [x] Static bz2: `BZ2_bzDecompress` in binary, no `libbz2` in `ldd`.
- [ ] `ac map_extractor <MPQ>` — no MPQ fixture available in this environment;
      covered indirectly by the static-bz2 symbol check.
- [x] `compile_commands.json` regenerated: 607 entries, CPATH mirrored as `-I`,
      76,441 include dirs checked, 0 missing.
- [x] clangd `--check=zig-build/Test.cpp` (includes `<mysql.h>` +
      `<openssl/evp.h>`) → "All checks completed, 0 errors".

## Follow-up — maintainer feedback applied

Direction confirmed: prefer vendoring; env vars are fine if standard; zig is
assumed/forced to be 0.16.

- **bzip2 vendoring confirmed** (finding #2 resolved — stay vendored; it is a
  private, invite-only server and build simplicity beats CVE tracking here).
- **`MYSQL_LIB_DIR` → `LIBRARY_PATH`** (standard, honoured natively by zig;
  verified: `linkSystemLibrary("mysqlclient", .no)` links with `LIBRARY_PATH`
  and fails without it). `linkSystemLibraries` is now pure stock zig with no
  custom env reads. Fail-fast now requires only `CPATH` — the cryptic failure
  was the missing header; Docker's default lib dir needs no `LIBRARY_PATH`.
- **zig forced to 0.16**: `comptime @compileError("...requires zig 0.16.x...")`
  in `zig-build/build.zig`, and the flake uses `pkgs.zig_0_16` instead of
  `pkgs.zig`.
- **deps audit**: `deps/zlib` is the only unreferenced vendored tree (the build
  uses system zlib) — either delete it or switch to it to drop the system zlib
  dep. Everything else is referenced but carries upstream build leftovers
  (`CMakeLists.txt`, `tools/`, `bindings/`, `win/`, `test/`) the zig graph never
  touches; pruning is mechanical and can be a separate change.

Re-verified after the follow-up:

- [x] fresh `nix develop`: `zig 0.16.0`, `CPATH` and `LIBRARY_PATH` exported;
      `zig build ac` exit 0.
- [x] `env -u CPATH zig build ac` → actionable "missing build environment
      variable CPATH" message.
- [x] Docker `linux/amd64` build exit 0; `ac` runs, all shared libs resolve,
      bz2 statically present.
