# MIGRATE_CONFIG_PLAN — remove .conf file loading, hardcode defaults, env-var config

> Status: planned, not started.
> Scope: `src-port/` only (the tree the zig build compiles). Legacy `src/` tree untouched — it dies with the cmake migration.
> Date: 2026-09-16

## Goal

Stop loading `worldserver.conf` / `authserver.conf` from disk entirely.

- All options hardcoded as arena-optimized defaults (instant-80 server) in C++.
- Database credentials come from env vars **only** (required, 12-factor style — no secrets in code).
- Redis host/port configurable via env vars (optional override, localhost defaults).

## Key research findings (verified 2026-09-16)

- **Env var support already exists and works without any conf file.**
  `src-port/common/Configuration/Config.cpp` — `GetValueDefault` (~line 381) checks env var
  `AC_<OPTION_NAME>` (e.g. `AC_WORLD_DATABASE_INFO`) *before* the loaded config map, even if the
  key was never loaded from a file. Key conversion via `IniKeyToEnvVarKey` (line 226) +
  `GetEnvVarName` (line 286, `AC_` prefix, upper snake case).
- **All ~534 `GetOption` call sites already carry a default value in code.** Do NOT touch call
  sites; swap the ConfigMgr backend instead.
- **Redis ignores config entirely today.** `src-port/common/Redis/RedisConn.cpp:19` hardcodes
  `127.0.0.1:6379`.
- **Log system has a fallback:** `src-port/common/Logging/Log.cpp` `ReadLoggersFromConfig()` —
  if no `Logger.root` key exists it builds default console loggers (root=Error, server=Info).
- **Fatal options** list in `Config.cpp:38`: `RealmID`, `LoginDatabaseInfo`, `CharacterDatabaseInfo`,
  `WorldDatabaseInfo` — currently only log FATAL and fall back to defaults (server then dies later
  in `DatabaseLoader::Load`).
- **Zig build never installs confs** — no zig-build changes needed. Only install steps are
  `addInstallArtifact` (zig-build/BuildCommons.zig:91,137).
- **dbimport is NOT part of the zig build.** `src-port/tools/` contains only the map/mmaps/vmap
  extractors. dbimport lives only in legacy `src/tools/dbimport` (AzerothCore DB bootstrap tool:
  creates auth/characters/world schemas from base SQL + applies `data/sql/updates/`). Decision:
  dropped, no action needed — it disappears with the old tree.
- **`GetKeysByString` users** (need keys seeded in the map, not call-site defaults):
  - `Log.cpp:187` `Appender.*`, `Log.cpp:196` `Logger.*`
  - `Metric.cpp:88` `Metric.Threshold.*` (metrics off by default → no keys needed)
- dist files: `src-port/worldserver/worldserver.conf.dist` (~525 entries, roughly a third unread
  by any call site), `src-port/auth/authserver.conf.dist`.
- Entrypoints that load conf: `src-port/worldserver/Main.cpp` (`worldserver_main`, ~line 128:
  `sConfigMgr->Configure` + `LoadAppConfigs`), `src-port/auth/Main.cpp` (~line 84-99, same
  pattern, `_ACORE_REALM_CONFIG`).

## Implementation steps

### 1. Config backend swap — `src-port/common/Configuration/`
- New `DefaultConfig.cpp` (next to `Config.cpp`): static list of `{name, value}` pairs with the
  curated defaults.
- `Config.cpp` surgery:
  - `LoadAppConfigs()` seeds `_configOptions` from the defaults list instead of
    `LoadInitial`/`ParseFile`.
  - Delete: `ParseFile`, `LoadFile`, `LoadInitial`, `LoadAdditionalFile`, `LoadModulesConfigs`,
    `IsAppConfig`, `GetConfigPath` (`_CONF_DIR`), `_filename` file handling,
    `CONFIG_ABORT_INCORRECT_OPTIONS` block.
  - Keep: `GetValueDefault` env-var lookup (works pre-map), `GetKeysByString`,
    `Reload()` → re-seed (makes `.reload config` a harmless no-op).
- Remove the `_fatalConfigOptions` list; seed `RealmID = 1` in defaults instead.
- Required env vars missing → one-line fatal with exact usage hint
  (`set AC_LOGIN_DATABASE_INFO=...`), replacing the old "add to config file" messaging.

### 2. Defaults content (baseline = current conf.dist values)
- Include only options actually read by call sites.
- Seed explicit `Appender.*`/`Logger.*` defaults (console appender, sane levels) so logging is
  deterministic instead of relying on the implicit Log.cpp fallback.
- Gameplay keys: distill baseline from conf.dist, then flag the ~20 gameplay keys (XP rates,
  start-level keys, arena/battleground settings, movement/rate tuning) for a decision on
  arena/instant-80 values. Rest verbatim from dist.

### 3. DB credentials — env required
- Do NOT put `LoginDatabaseInfo`/`CharacterDatabaseInfo`/`WorldDatabaseInfo` in the defaults map.
- Startup validation (before `DatabaseLoader::Load`, see `StartDB()` in
  `src-port/worldserver/Main.cpp`): all three `AC_*_DATABASE_INFO` env vars must be set,
  otherwise fatal with usage hint. `AC_REALMID` optional override (default 1).
- Same for authserver (`AC_LOGIN_DATABASE_INFO`).

### 4. Redis — `src-port/common/Redis/RedisConn.cpp`
- Replace hardcoded `127.0.0.1:6379` with `AC_REDIS_HOST` / `AC_REDIS_PORT`
  (localhost defaults kept as fallback so local dev still works).

### 5. Entrypoints + cleanup
- `src-port/worldserver/Main.cpp` + `src-port/auth/Main.cpp`: remove `configFile` path
  construction, `--config`/`-c` option, `GetConsoleArguments` config plumbing; keep
  `--help` / `--version`.
- Delete `src-port/worldserver/worldserver.conf.dist` and `src-port/auth/authserver.conf.dist`.
- Zig build: no changes.

### Suggested order
1+3 first (Config backend + env validation), then 2 (defaults distillation + gameplay-key
flag list), then 4, then 5 (entrypoints + file deletion).

## Risks / notes

- Without seeding the full map, every missing key logs a WARN per `GetOption` call at startup
  (thousands of lines). Step 2 avoids this — seed everything that is read.
- Env-var typos (`AC_WORLD_DB_INFO` vs `AC_WORLD_DATABASE_INFO`) become startup failures.
  Acceptable for a dedicated deployment; step 3 validation makes it loud.
- `.reload config` CLI command becomes a no-op (re-seeds defaults). Keep or remove the command —
  decision at implementation time.
- Another agent is actively working on the cmake→zig migration in this repo — coordinate before
  editing files under `src-port/common/Configuration/` and the two Main.cpp files.
