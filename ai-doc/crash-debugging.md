# Crash debugging

How a hard crash (segfault/abort) is captured and how to turn it into a source
location.

## What runs

`Acore::InstallCrashHandler("<app>")` is called at startup by `worldserver`
(`src/worldserver/Main.cpp`) and `authserver` (`src/auth/Main.cpp`), right after
the env file is applied. It installs POSIX signal handlers for `SIGSEGV`,
`SIGBUS`, `SIGILL`, `SIGFPE` and `SIGABRT` (see
`src/common/Debugging/CrashHandler.cpp`).

Asserts are covered too: `Acore::Assert/Fatal/Error` write to a null pointer on
Linux, which raises `SIGSEGV`.

On a fatal signal the handler writes a report to **stderr** (so it shows up in
`docker compose logs world`) and, when `AC_CRASH_DIR` is set, to a timestamped
file `<app>_crash_<YYYYmmdd_HHMMSS>_<pid>.log` in that directory. Compose mounts
`./crashes` from the host and sets `AC_CRASH_DIR=/app/crashes`, so reports also
land in `./crashes/` on the host.

## Report contents

```
=== worldserver crashed ===
signal:   11 (SIGSEGV)
revision: <git describe>
pid:      <pid>
tid:      <thread id>
report:   /app/crashes/worldserver_crash_..._<pid>.log

fault address: 0x0
si_code:       1 (SEGV_MAPERR (address not mapped))

registers (x86_64):            # or aarch64 in production
  rip: ... rsp: ... rbp: ...
  ...

raw addresses:
  #0  0x...
  ...
backtrace:
  /usr/local/bin/ac(+0x...)[0x...]
  ...
```

The faulting thread id is useful because the world uses several threads.

## Quick triage

- `fault address: 0x0` (or a small offset like `0x8`, `0x18`) means a
  null-based access. A defensive check at the call site is a valid hotfix —
  but log it and find why the pointer was null (a `null` is usually a symptom:
  missing row, failed lookup, uninitialized member, object not yet in world).
- `fault address` non-zero and bogus with `SEGV_MAPERR` → wild pointer.
  `SEGV_ACCERR` → the page exists but wasn't accessible (often a
  use-after-free / dangling pointer). Don't just null-check those; they are
  lifecycle bugs.
- `si_code` for `SIGABRT` sent by `abort()`/`raise()` is `SI_TKILL`/`SI_USER`;
  `si_addr` is then not a real fault address.
- The register block is architecture specific: dev is usually `x86_64`,
  production is `aarch64`.

## Reading the backtrace

`backtrace:` lines are `binary(function+offset) [address]`. Names are C++
mangled when the compiler didn't demangle them; pipe through `c++filt`:

```
c++filt                    # paste a line
sed 's/.*ac(//; s/+.*//' | c++filt
```

The `raw addresses:` are the quickest path to source. The `ac` binary is built
**non-PIE** and **not stripped**, so runtime addresses are file addresses:

```
# exact file:line for a frame
addr2line -e zig-out/bin/ac -f -C -a 0x<address>

# or inside gdb
gdb zig-out/bin/ac -ex 'list *0x<address>'
```

## Reproducing under gdb

```
nix develop
zig build ac
gdb --args ./zig-out/bin/ac worldserver
(gdb) run
# ...crash...
(gdb) bt full
(gdb) thread apply all bt
```

If you only have the report (no live repro), rerun with the printout in hand and
match function names.

## Matching the binary

Symbols only resolve against the **same revision** the report came from. The
report (and the startup banner) prints the git revision; rebuild that commit
before symbolizing.

## Build requirements (already wired)

For readable traces the build keeps frame pointers and exports symbols:

- `-fno-omit-frame-pointer` in `zig-build/Src.zig` (`core_cflags`)
- `exe.rdynamic = true` in `zig-build/BuildCommons.zig`
- debug info is not stripped (Zig default)

If you ever change those, the handler still writes raw addresses but names may
disappear.

## Optional: core dumps

Core dumps in containers are awkward because `kernel.core_pattern` is a
host-global, usually piped to `systemd-coredump`, so `ulimit -c unlimited`
alone often produces no file inside the container. If we ever need them, set
`kernel.core_pattern` on the host to a bind-mounted path and add
`ulimits: core: -1` to the service. The backtrace handler is intended to make
this unnecessary for typical crashes.
