# Zig Build Migration

Living document for the CMake -> Zig build migration. Read this before starting a
session. Update it as decisions land; do not delete entries from the decisions
log, append amendments instead.

## Current state (Phase 0 - DONE)

Zig build (`zig-build/`) with the `AcGraph` / `Deps.zig` architecture:

- Build entry: root `build.zig` delegates to `zig-build/build.zig` (`runBuild`).
- Dependencies live in `zig-build/Deps.zig`, each dep is a `module + library`
  pair with a `link*` helper that consumers use. Include/define knowledge is
  centralized there. Consumers never add include paths manually - libs expose
  headers via `installHeadersDirectory` (Zig 0.16 propagates them through
  `linkLibrary`).
- Ported modules live in `zig-build/Src.zig` (AcGraph holds it under `src`,
  next to `deps`), same pattern: common/database/shared/auth/tools are
  `module + library` pairs with `link*` helpers that CASCADE like CMake
  PUBLIC propagation (`linkAuth` gives you shared->database->common includes
  and libs). Core cflags (C++23 + deprecation suppressions) are defined once
  as `Src.core_cflags`.
- Source querying via `cppkit-zig` (`cpp.querySources` + `filterOut*`).

| Dependency | Type | Status | Notes |
|---|---|---|---|
| utf8cpp | vendored | done | headers compiled as TUs, `cpp17.h` filtered |
| argon2 | vendored | done | `-DARGON2_NO_THREADS=1`, arch-based ref/opt filter |
| detour | vendored | done | `-std=c++14` |
| fmt | vendored | done | `.cc` sources, `FMT_CONSTEVAL=constexpr`, `fmt.cc` filtered |
| boost | zon (`allyourcodebase/boost-libraries-zig` v1.91) | done | headers-only usage; `linkBoost` mirrors artifact `include_dirs` + 8 `BOOST_*` defines. **No `dll` module** (see decisions) |
| openssl, hiredis, mysqlclient | system (nix) | done | `linkSystemLibrary` via pkg-config; mysqlclient resolves to nix `mariadb-connector-c` |
| SFMT, jemalloc, gperftools, stdfs, threads, jsonpath | - | dropped | unused / no-op / optional |
| zlib, g3dlite, recast, gsoap, readline | vendored/system | pending | needed by worldserver/tools phases |
| bzip2, libmpq | vendored | pending | needed by extractor tools phase |

Verification tooling in place:

- `zig build test-build` - linking smoke test exercising every dependency with
  real symbol usage (`zig-build/Test.cpp`).
- `zig build compile-commands` - emits `compile_commands.json` (gitignored) via
  `cppkit-zig` for clangd. New targets must call `cpp.addCompileCommands(exe)`.
- Builds must run inside `nix develop` (pkg-config + system libs).

## Target architecture

### `src-port/`

New source root. Sources are **selectively** pulled in from `src/` as they are
ported - no bulk copy. The old `src/` stays untouched as the reference until a
phase is complete. Expect restructure, not translation: this is the moment to
drop dead code (see decisions).

Rough layout (adjust while porting):

```
src-port/
  common/        # from src/common (Cryptography, Database? no - database is separate, Logging, Utilities, ...)
  database/      # from src/server/database, minus everything update/migration
  shared/        # from src/server/shared
  auth/          # from src/server/apps/authserver (Main, Authentication, Server)
  world/         # from src/server/apps/worldserver + game glue it needs
  tools/         # map_extractor, mmaps_generator, vmap4_extractor, vmap4_assembler
```

### `ac` master executable

Single binary, subcommand dispatch (`zig build` installs `zig-out/bin/ac`):

| Subcommand | Source | Notes |
|---|---|---|
| `ac authserver` | src-port/auth | replaces the authserver app |
| `ac worldserver` | src-port/world | replaces the worldserver app |
| `ac map_extractor` | src-port/tools | needs libmpq + bzip2 + zlib |
| `ac mmaps_generator` | src-port/tools | needs recast + detour + g3dlite |
| `ac vmap4_extractor` | src-port/tools | needs libmpq + bzip2 |
| `ac vmap4_assembler` | src-port/tools | |

- CLI parsing: **p-ranav/argparse** (header-only). Prior art: vendored at
  `src/auth/Include/argparse.h` in commit `361dfc0b2` on the old experiment
  branch - rescue it from git history into `src-port/common` (or a
  `src-port/third_party` dir).
- Unified world+auth process is **deferred**: `ac authserver` and
  `ac worldserver` run separately for now (keeps configs and startup simple).
  Unification is a later, opt-in refactor.

### Scope removals

- **No DB migration machinery in C++.** `DBUpdater`, `UpdateFetcher` and the
  update-apply paths are not ported. `DatabaseLoader` ports without the update
  fetcher hookup. SQL/schema management moves to a scripting layer (TypeScript
  + bun) at the very end of the migration.
- `dbimport` tool is dropped (same reasoning - base SQL bootstrap becomes a
  script concern).
- Docker-compose provides MySQL + Redis for local runs. **Do not run any
  docker commands during the port phases** - container wiring happens at the
  end.

### Configs

Keep the existing conf files as-is for now (`authserver.conf.dist`,
`worldserver.conf.dist`). Revisit when/IF the server processes unify.

## Decisions log

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
4. Known consteval blockers from the old build, apply filters if they resurface:
   `DatabaseLoader.cpp`, `DatabaseWorkerPool.cpp`, `Field.cpp`
   (DBUpdater/UpdateFetcher are dropped outright now). Old patches to
   `MPSCQueue.h` / `QueryHolder.h` are already in tree.
5. Deps.zig conventions: vendored dep = `querySources` + `addCSourceFiles` +
   `installHeadersDirectory` + `link*` helper; system dep = `link*` helper with
   `linkSystemLibrary`; boost = zon artifact + include-dir mirroring.
6. Every ported lib gets its own `zig build <name>` step for isolated
   verification, plus smoke-test / compile-commands registration.
7. Git: the assistant does NOT commit; the human commits after review.
8. Old branch history (`a86cff049` "got common lib to build", `706edd85e`
   "add database static library", `62fa5a143` "auth-server (kind of) builds")
   is the reference for known source-level fixes - reuse those lessons.
9. **`ac` router architecture** (`src-port/main.cpp`): argparse validates the
   sub command name only, then forwards `argv+1` 1:1 to the renamed tool
   `main()`s (declared in `src-port/subcommands.h`). Tools keep their own arg
   parsing, so usage strings and flags are identical to the old standalone
   binaries. `ac <bad command>` prints the router help.
10. **asio define deviation**: `BOOST_ASIO_NO_DEPRECATED` (from CMake's boost
    interface) is deliberately NOT set - with asio 1.91 it strips
    `basic_deadline_timer` members that common's `DeadlineTimer` wrapper uses.
11. **g3dlite patch**: `System::free()` got the same lazy `initMem()` guard its
    malloc/realloc siblings have; without it, static destructors freeing G3D
    memory before any G3D allocation crashed on a null `BufferPool` (bare
    `ac` invocation).
12. **tool source consolidation** (needed because all tools now link into ONE
    binary): the duplicated `dbcfile.{h,cpp}` / `mpq_libmpq04.h` /
    `mpq_libmpq.cpp` copies of map_extractor + vmap4_extractor moved to
    `src-port/tools/shared/` (vmap4 variant, `close()` made public again);
    map_extractor's `FileExists` is `static`; vmap4's globals
    (`input_path`/`output_path`/`map_ids`) prefixed with `vmap_`.
13. **Flat-name includes**: common and the tools rely on every subdirectory
    being on the include path (CMake `CollectIncludeDirectories` behavior).
    Replicated via `Deps.addFlatIncludes()` - dirs are sorted so same-named
    headers resolve deterministically (`map_extractor/loadlib` wins over
    `vmap4_extractor/loadlib`, matching CMake).
14. boost zon dep runs with `.filesystem = true` (common's TileAssembler
    needs it); `zig-pkg/` (boost sub-package extraction) is gitignored.
15. AcGraph mirrors the ported module structure: `common` (and later
    database/shared/...) live as direct AcGraph members built by
    `BuildCommons.zig`; `Deps.zig` holds third parties only (argparse moved
    there too, at `deps/argparse` - header-only, linked via include-dir
    propagation). Flat-include dirs helper was factored into cppkit
    (`cpp.addFlatIncludes`).
16. Banner art replaced with ARENA/CRAFT (same slanted ANSI-shadow glyphs,
    extracted from the original banner); tagline: "ArenaCraft - based on
    AzerothCore 3.3.5a".
17. **libmysqlclient, NOT mariadb-connector**: mariadb's mysql.h forces
    `__cpp_nontype_template_args` down to 201411L (breaks fmt 11 under
    C++23) and lacks `mysql_ssl_mode`/`mysql_stmt_bind_named_param`. The
    flake now uses `mysql84` with `MYSQL_INCLUDE_DIR`/`MYSQL_LIB_DIR` env
    vars consumed by `Deps.linkMysqlClient` (no pkg-config for this one).
18. **core module cflags**: `-Wno-deprecated-literal-operator` (fmt 11
    fallback shim under zig 0.16's clang) and `-Wno-deprecated-declarations`
    (newer asio marks `basic_deadline_timer`/`null_buffers` deprecated;
    AC's DeadlineTimer/Socket wrappers inherit/use them - warnings only,
    suppressed deliberately).
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
    configure_file generation (ScriptLoader.cpp: AddCommandsScripts()...
    AddWorldScripts(); ModulesLoader.cpp: Addmod_arenacraftScripts() +
    Addmod_duel_resetScripts()). The CMake INTERFACE macros
    `AC_MODULES_LIST` / `CONFIG_FILE_LIST` are defined on the worldserver
    module in Src.zig (trailing commas are part of the format).
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

## Phased roadmap

Order is bottom-up: deps -> libs -> apps. Each phase should end with a green
`zig build test-build` (or its successor) and the phase's build step.

- **Phase 0 (done)**: dependency layer (see table above) + smoke test +
  compile-commands.
- **Phase 1 (done)**: `src-port/` skeleton + **entire** `src/common` ported to
  `src-port/common` (C++23, links fmt/boost/openssl/hiredis/argon2/utf8/detour/
  g3dlite) + `recast`, `g3dlite`, `mpq` deps + zlib/bzip2 system helpers +
  `ac` master executable with the four map tools working 1:1 (see decisions
  9-12). Steps: `zig build common` (lib in isolation), `zig build ac`.
  Excluded from the port: `Platform/` (Win32 only), `Debugging/
  WheatyExceptionReport` (Win32 only), `Utilities/StartProcess` (dropped,
  boost/iostreams consumer).
- **Phase 2 (done)**: `src-port/database` - full database lib, DBUpdater/
  UpdateFetcher dropped, `DatabaseLoader` stripped of update hooks (explicit
  instantiations kept, so `AddDatabase<T>` links 1:1). Links common +
  libmysqlclient. Step: `zig build database`.
- **Phase 3 (done)**: `src-port/shared` - Realms, Secrets, DataStores,
  ByteBuffer, SharedDefines (Network is header-only). Links database.
  Step: `zig build shared`.
- **Phase 4 (done)**: `src-port/auth` - Main.cpp entry renamed to
  `authserver_main`, `GetConsoleArguments` rewritten from
  boost::program_options to argparse (same flags: -h/--help, -v/--version,
  -d/--dry-run, -c/--config; unknown args still tolerated because
  sConfigMgr->Configure() re-scans full argv). `ac authserver` works
  end-to-end: config load -> banner -> SSL/Boost info -> MySQL connect
  (fails without docker, as expected).
- **Phase 5 (done)**: `src-port/worldserver` + `src-port/game` (290 sources)
  + `src-port/scripts` (105) + `src-port/modules` (mod-arenacraft,
  mod-duel-reset). CMake's configure_file loader generation replaced with
  hand-written `src-port/scripts/ScriptLoader.cpp` and
  `src-port/modules/ModulesLoader.cpp` (static module set - keep in sync
  when adding script dirs/modules). New deps: gsoap (vendored), readline
  (nix). `ac worldserver` compiles and starts 1:1: config -> module configs
  -> banner -> dies at Redis connect (docker phase). Phases 6-7 unchanged
  (tools already landed in Phase 1).
- **Phase 7 - cleanup & runtime**: prune unused `deps/` vendors, docker-compose
  wiring for mysql/redis, bun/TS scripting layer for SQL management, unified
  process decision (defer), config consolidation (defer).

## Open questions

- Executable/module naming inside `zig build` (e.g. `zig build ac` producing
  `zig-out/bin/ac` vs one lib step per phase). Lean towards building the `ac`
  exe from Phase 4 onward with subcommands appearing as they port.
- Do tools keep their exact old CLI flags (argparse rewrite should mirror
  them for muscle memory)?
- `revision.h`: keep placeholder values (old approach) or run `git describe`
  at build time via a zig Run step?
- jemalloc/gperftools: dropped for good, or keep as opt-in `--build-option`
  later? (currently: dropped)
