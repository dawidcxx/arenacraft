# Zig Build Migration — Hand-off Document

Status: **the port is complete and building**. This document is the entry
point for anyone (human or AI) continuing development, now primarily on
**nix / Linux**. Read this fully before changing anything.

Last verified state (macOS host, zig 0.16.0, nix dev shell):

- `zig build ac` - full stack: common -> database -> shared -> auth/game/
  scripts/modules/worldserver + tools, all routed through ONE binary
- `./zig-out/bin/ac authserver` - starts 1:1 like upstream (config -> banner
  -> SSL/Boost info -> dies at MySQL connect: no DB running yet)
- `./zig-out/bin/ac worldserver` - starts 1:1 (config -> banner -> dies at
  Redis connect: no Redis running yet)
- `./zig-out/bin/ac map_extractor|vmap4_extractor|vmap4_assembler|
  mmaps_generator` - behave exactly like the old standalone tools
- `zig build test-build` - linking smoke test exercising every dependency
- `zig build compile-commands` - compile_commands.json for clangd (gitignored)

## How the build is structured

Root `build.zig` -> `zig-build/build.zig` (`runBuild`) -> `AcGraph`
(`zig-build/BuildCommons.zig`) which owns two sub-graphs:

- **`Deps` (`zig-build/Deps.zig`)** - third parties only. Each dep is a
  `module + library` pair built by a `build*` fn, consumed via `link*`
  helpers that encapsulate include paths + defines. Vendored: utf8cpp,
  argon2, detour, fmt, mpq, recast, g3dlite, gsoap, argparse (header-only).
  System (nix/pkg-config): openssl, hiredis, readline, zlib, bzip2, and
  **libmysqlclient** (special path, see decision 17).
- **`Src` (`zig-build/Src.zig`)** - our ported sources under `src-port/`:
  common, database, shared, auth, tools, game, scripts, modules, worldserver.
  Same pattern; `link*` helpers CASCADE like CMake PUBLIC propagation
  (`linkAuth` = auth lib + shared->database->common libs+includes). Core
  cflags (C++23 + deprecation suppressions) live in `Src.core_cflags`.
- Exe targets live in BuildCommons: `ac` (argparse router in
  `src-port/main.cpp` + `src-port/subcommands.h`, forwards argv 1:1 to
  `*_main` entry points) and `test-build`.
- Source querying via `cppkit-zig` (`cpp.querySources`, `cpp.addFlatIncludes`
  - the latter puts a dir + every subdir on the include path, replicating
  CMake's `CollectIncludeDirectories`; needed because AC uses flat-name
  includes across subdirectories).
- Every ported module registers an isolated `zig build <name>` step
  (common, database, shared, game) for triage.

`src-port/` is **fully self-contained**: nothing references the old `src/`
tree anymore. Old-tree content was copied, not linked.

## Environment (important)

- Build ONLY inside `nix develop` (flake.nix): provides zig, pkg-config and
  the C libraries. Outside it, pkg-config resolution of openssl/hiredis
  fails.
- The flake exports `MYSQL_INCLUDE_DIR` / `MYSQL_LIB_DIR` (nix `mysql84`)
  consumed by `Deps.linkMysqlClient` via `mod.owner.graph.environ_map`.
- On a **Linux nix host**: replicate the same flake pattern with linux
  nixpkgs libs; `zig build` natively should work once pkg-config/env resolve
  (the C++ sources themselves are platform-clean, see Linux findings below).
- `docker-compose.yml` is ready (mysql:8.0 + valkey:8.0 + redis-insight).

## Linux / cross-compile findings (from the macOS host)

Attempted `zig build -Dtarget=x86_64-linux-{gnu,musl}` cross-compile:

- `gnu` target: fails immediately - glibc headers are not bundled; musl is
  the zero-friction zig target.
- `musl` target: vendored C deps hit missing libc headers (argon2:
  `string.h`/`stdio.h` not found) and vendored g3dlite has real portability
  issues under stricter clang: incomplete `struct timeval` (missing
  `<sys/time.h>` include in G3D/System.h) and `-Wenum-enum-conversion`
  errors in System.cpp.
- Additionally, the system libs (openssl/hiredis/libmysqlclient/readline/
  zlib/bzip2) resolve through the HOST pkg-config/env - cross-linking needs
  a linux sysroot; from macOS that is a nix `pkgsCross` exercise.

**Conclusion / chosen path**: build natively on the Linux nix host instead
of cross-compiling. The compile failures above are all in vendored deps and
are mechanical fixes (missing includes / warning-as-error); expect to touch
`deps/g3dlite` the same way `deps/g3dlite/source/System.cpp` was already
patched (see decision 11). The ported `src-port` sources themselves showed
no platform-specific compile errors up to the point the deps failed.

## Decisions log (append-only; do not delete entries)

1. C++ standard is **C++23** for all ported core libs (old zig build proved
   C++20 consteval issues with libc++; CMake says 20, we don't care).
2. boost comes from the `allyourcodebase/boost-libraries-zig` zon package
   (0.16-native). `boost::program_options` (old authserver Main.cpp) and
   `boost::dll` (OpenSSLCrypto.cpp runtime_symbol_info) are NOT available in
   the package: program_options is replaced by argparse, dll is replaced
   during the port (executable path resolution via platform APIs /
   std::filesystem, or drop the crash-log feature it feeds).
3. `StartProcess.cpp` (boost/iostreams consumer, only referenced by the
   removed DBUpdater) is not ported.
4. Known consteval blockers from the old build turned out to be gone on
   zig 0.16 + newer libc++: `Field.cpp`, `DatabaseWorkerPool.cpp`,
   `DatabaseLoader.cpp` all compile now (old build filtered them; we don't).
   Old patches to `MPSCQueue.h` / `QueryHolder.h` are already in tree.
5. Deps.zig conventions: vendored dep = `querySources` + `addCSourceFiles` +
   `installHeadersDirectory` + `link*` helper; system dep = `link*` helper
   with `linkSystemLibrary`; boost = zon artifact + include-dir mirroring.
6. Every ported lib gets its own `zig build <name>` step for isolated
   verification, plus smoke-test / compile-commands registration.
7. Git: the assistant does NOT commit; the human commits after review.
8. Old branch history (`a86cff049` "got common lib to build", `706edd85e`
   "add database static library", `62fa5a143` "auth-server (kind of) builds")
   is the reference for known source-level fixes - reuse those lessons.
9. **`ac` router architecture** (`src-port/main.cpp`): argparse validates the
   sub command name only, then forwards `argv+1` 1:1 to the renamed
   `main()`s (declared in `src-port/subcommands.h`). Sub programs keep their
   own argument parsing, so usage strings and flags are identical to the old
   standalone binaries.
10. **asio define deviation**: `BOOST_ASIO_NO_DEPRECATED` (from CMake's boost
    interface) is deliberately NOT set - with asio 1.91 it strips
    `basic_deadline_timer` members that common's `DeadlineTimer` wrapper
    uses.
11. **g3dlite patch**: `System::free()` got the same lazy `initMem()` guard
    its malloc/realloc siblings have; without it, static destructors freeing
    G3D memory before any G3D allocation crashed on a null `BufferPool`.
12. **tool source consolidation** (all tools link into ONE binary): the
    duplicated `dbcfile.{h,cpp}` / `mpq_libmpq04.h` / `mpq_libmpq.cpp`
    copies of map_extractor + vmap4_extractor moved to
    `src-port/tools/shared/` (vmap4 variant, `close()` made public again);
    map_extractor's `FileExists` is `static`; vmap4's globals
    (`input_path`/`output_path`/`map_ids`) prefixed with `vmap_`.
13. **Flat-name includes**: common and the tools rely on every subdirectory
    being on the include path (CMake `CollectIncludeDirectories` behavior).
    Replicated via `cpp.addFlatIncludes` - dirs are sorted so same-named
    headers resolve deterministically (`map_extractor/loadlib` wins over
    `vmap4_extractor/loadlib`, matching CMake).
14. boost zon dep runs with `.filesystem = true` (common's TileAssembler
    needs it); `zig-pkg/` (boost sub-package extraction) is gitignored.
15. AcGraph mirrors the ported module structure: `Deps.zig` holds third
    parties, `Src.zig` holds our modules (common/database/shared/auth/
    tools/game/scripts/modules/worldserver). The `ac` exe targets stay in
    BuildCommons.zig.
16. Banner art replaced with ARENA/CRAFT (same slanted ANSI-shadow glyphs,
    extracted from the original banner); tagline: "ArenaCraft - based on
    AzerothCore 3.3.5a".
17. **libmysqlclient, NOT mariadb-connector**: mariadb's mysql.h forces
    `__cpp_nontype_template_args` down to 201411L (breaks fmt 11 under
    C++23) and lacks `mysql_ssl_mode`/`mysql_stmt_bind_named_param`. The
    flake now uses `mysql84` with `MYSQL_INCLUDE_DIR`/`MYSQL_LIB_DIR` env
    vars consumed by `Deps.linkMysqlClient` (no pkg-config for this one).
    On Linux, use the distro/nix real libmysqlclient equivalently.
18. **core module cflags** (`Src.core_cflags`): `-Wno-deprecated-literal-
    operator` (fmt 11 fallback shim under zig 0.16's clang) and
    `-Wno-deprecated-declarations` (newer asio marks `basic_deadline_timer`/
    `null_buffers` deprecated; AC's DeadlineTimer/Socket wrappers inherit/
    use them - warnings only, suppressed deliberately).
19. **libc++ drift header fixes** in ported sources: `DatabaseEnvFwd.h`
    gained `#include <memory>`, `ByteBuffer.h` gained
    `#include <type_traits>` (newer libc++ no longer provides these
    transitively).
20. **Config files resolve relative to the executable**: `ConfigMgr::
    GetConfigPath()` returns the exe's directory (portable helper:
    `_NSGetExecutablePath` / `/proc/self/exe` / `GetModuleFileName`), so
    `/foo/bar/ac authserver` loads `/foo/bar/authserver.conf`. The
    `_CONF_DIR` build macro is gone.
21. **modules/scripts loaders are hand-written**, replacing CMake's
    configure_file generation (`src-port/scripts/ScriptLoader.cpp`: 
    AddCommandsScripts()... AddWorldScripts(); `src-port/modules/
    ModulesLoader.cpp`: Addmod_arenacraftScripts() +
    Addmod_duel_resetScripts()). Keep them in sync when adding script
    dirs/modules.
22. **TU-local helpers static-ified** in both server Mains: StartDB/StopDB/
    GetConsoleArguments were defined with identical names in auth and
    worldserver Main.cpp; with everything in one binary they must be
    `static`.
23. Modules Lua/eluna machinery from CMake was skipped outright - the two
    in-tree modules don't use it.
24. **Module configs dropped entirely, values hardcoded**: duelreset.conf.dist
    was redundant with the in-code defaults, so `DuelReset::LoadConfig` uses
    constants, the `CONFIG_FILE_LIST`/`AC_MODULES_LIST` cmake macros are gone
    (module list inlined in worldserver Main.cpp - it feeds the
    `.server modules` chat command and nothing else since DB updates are
    dropped), and `sConfigMgr->LoadModulesConfigs()` is no longer called.
    No `modules/` config dir is needed next to the executable.
25. **Cross-compile from macOS to Linux is abandoned** in favor of native
    builds on the Linux nix host (see "Linux / cross-compile findings"
    above for the exact failures the next person will re-hit).
26. **Linux port fixes landed (2026-09-16)**: (a) the two C-only deps
    (`argon2`, `mpq`) were missing `link_libc = true` in `Deps.zig` - the
    "argon2 header resolution is a zig-target quirk" above was actually
    this; macOS auto-resolves SDK headers, NixOS has no /usr/include to
    fall back on. (b) zig's bundled libc++ hardcodes `long long` chrono
    reps while linux `int64_t` is `long` (on apple they coincide, which is
    why the mac build linked). Global fix: `Acore::Types::canonical()` in
    `PreparedStatement.h` maps `long long`/`unsigned long long` onto the
    typedef'd types at the single `SetData` funnel; the duration overload
    now always converts to `uint32` (no caller ever passed
    `convertToUin32 = false`, and the old ternary promoted to `long long`
    even on the convert path). (c) three raw `GetGameTime().count()`
    `SetData` call sites got upstream's `uint32` cast (Pet.cpp, 
    TicketHandler.cpp, cs_quest.cpp).
27. **Stale .zig-cache on header-only changes**: while debugging the libc++
    rep mismatch, `zig build` kept linking old objects (same archive hash
    across runs despite edited headers). If a link error references a
    symbol signature you already fixed, `rm -rf .zig-cache` before
    questioning your sanity.

## Roadmap (what remains)

### R1 - Linux native build (DONE, verified 2026-09-16)
`zig build ac` succeeds on the nix linux host (zig 0.16.0). Smoke-tested
1:1 like the mac: authserver banner -> dies waiting on MySQL; worldserver
banner -> dies at Redis connect (`Connection refused`); tools run (no
client data yet, so "No locales detected" from map_extractor is normal).
Two mac->linux source class issues had to be fixed, see decisions 26/27.
Minor linux-only noise: "Can't set process priority class" warning on
worldserver startup is cosmetic.

### R2 - Runtime bring-up (docker + bootstrap)
- `docker compose up -d db valkey` (mysql + valkey; credentials in
  docker-compose.yml)
- **Bun/TS bootstrap scripts** (the replacement for the dropped
  dbimport/DBUpdater): create `acore_auth`, `acore_characters`,
  `acore_world` and apply `data/sql/base/db_*` schemas (654MB of base SQL,
  already in tree)
- Then chase: `ac authserver` reaching LISTENING state; `ac worldserver`
  past Redis -> next gate is game client data

### R3 - Client data via our own tools
`ac map_extractor` / `vmap4_extractor` / `vmap4_assembler` /
`mmaps_generator` against a 3.3.5a client (user provides the client).
worldserver will refuse to start without maps/vmaps/DBC data.

### R4 - Old-tree removal
`src-port/` is self-contained (verified: zero references to `src/`). Once
the Linux host builds and the servers boot against docker:
- delete `src/` (the entire old source tree)
- delete root `CMakeLists.txt`, `src/cmake/`, `acore.json`, `conf/`
  (only contains CMake configure scripts - the runtime templates live at
  `src-port/auth/authserver.conf.dist` + `src-port/worldserver/worldserver.conf.dist`)
  and all `*/CMakeLists.txt` leftovers
- `modules/` root CMake machinery can go; `modules/mod-*/src` content is
  already copied into `src-port/modules`
- KEEP: `data/` (base SQL for bootstrap), `docker-compose.yml`,
  `deps/` (vendored sources)
- prune unused `deps/` vendors afterwards: SFMT, jemalloc, gperftools,
  jsonpath, stdfs, threads (all unused; verify with rg before deleting)

### R5 - Small polish items
- `revision.h`: real `git describe` via a zig Run step (currently
  placeholders, see `Src.zig` buildCommon)
- `-Doptimize` release-build verification
- deferred: unified `ac server` (world+auth one process), whether the main
  confs (`worldserver.conf`/`authserver.conf`) get the module-config
  treatment (hardcoded) too

## Gotchas quick list for the next AI

- Build inside `nix develop`, always.
- Do not reintroduce mariadb-connector (decision 17) - it poisons
  `__cpp_nontype_template_args` and lacks mysql8 APIs.
- `src-port/modules/mod-*/src` files reference game headers flat; the
  cascade order in `Src.zig` link helpers matters (`linkGame` after
  `linkShared` etc.). Follow the existing pattern.
- When adding a script dir or module: copy sources into `src-port/...` AND
  update the hand-written loader cpp (decision 21).
- After adding a new build target: register `cpp.addCompileCommands(exe)`
  and extend `zig-build/Test.cpp` + `test-build` if it introduces a dep.
- `zig-cache/`, `zig-out/`, `zig-pkg/`, `*.a` are gitignored artifacts.
