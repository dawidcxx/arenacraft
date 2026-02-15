# AGENTS.md

## Quick working notes (Zig + project structure)

### Zig workflow
- Build: `zig build`
- List steps: `zig build -l`
- Run auth app: `zig build run-auth`
- Refresh compile DB: `zig build compile-commands`

### Build config pointers
- Auth build config lives in `build.zig` (`buildAuth`).
- Auth sources are collected from `src/auth/` recursively.
- Include path should point at `src/auth/Include`.
- System libs currently linked: OpenSSL + `mysqlclient`.

### Repo structure tips
- Keep auth code split by interface/implementation:
  - headers: `src/auth/Include`
  - source: `src/auth/Impl`
- Keep `src/auth/Main.cpp` as bootstrap/wiring only.
- Put protocol/session flow in `AuthSession`.
- Put SRP math/verification in `Srp6Server`.
- Put DB access and async wrappers in `MySqlAsync`.

### Practical notes
- `zig build` is green.
- `zig build run-auth` currently fails at runtime due to OpenSSL shared-library version mismatch in the environment.
- If runtime fails before app logs appear, check library/runtime linkage first.
