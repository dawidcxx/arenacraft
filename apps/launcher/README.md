# launcher

Windows client launcher for Arenacraft. Zig against the Win32 API through the
MinGW-w64 headers bundled with Zig; no Windows SDK or C toolchain.

The launcher targets `x86_64-windows-gnu` only, so `build.zig` exposes no
`-Dtarget`.

## Build

```sh
zig build launcher        # zig-out/bin/launcher.exe
zig build                 # same thing
```

The PE uses the GUI subsystem by default. `-Dconsole-window=true` switches to
the console subsystem, which is the only way to see `std.log` on Windows; wine
forwards stdout/stderr to the launching terminal either way.

## Run

```sh
nix-shell -p wineWow64Packages.stable \
  --run 'nix develop ../.. --command zig build run-launcher -fwine'
```

`-fwine` makes Zig execute the artifact through wine. On a host with a
binfmt_misc handler for PE files, plain `zig build run-launcher` works.
Arguments after `--` are forwarded to the program.

wine is not in the dev shell. Note that `pkgs.wine` is 32-bit-only and cannot
load an x86_64 PE; `wineWow64Packages` can. Use a dedicated `WINEPREFIX` if
`~/.wine` was created by the 32-bit build.

## Tests

```sh
nix-shell -p wineWow64Packages.stable \
  --run 'nix develop ../.. --command zig build test -fwine'
```

`lib` tests are Windows PE and need the wine executor above. The `clay` layout
tests are pure Zig with no libc or Win32 dependency, so they build and run
natively: `zig build clay-test` needs no wine, and `zig build test` runs both.
`zig build lib-test` runs the lib tests on their own.

## Icon

`launcher.rc` embeds `AppIcon.ico` in the PE, so `launcher.exe` carries the
application icon for Explorer, the taskbar, Alt+Tab and shortcuts; `gui` loads
the same resource for the window class. `AppIcon.ico` is generated from
`AppIcon.jpeg` by `scripts/make_icon.py` (needs `ffmpeg` on `PATH`); regenerate
and commit it when the artwork changes.

## What it does

The window declares a single-column layout: an **Arenacraft** title, a
description that re-wraps when the window narrows, a spacer, and a bottom row
with **Launch Game** and **Quit** buttons aligned to the right. Resize the
window and everything adapts; there are no hardcoded widget positions.

**Launch Game** resolves `Wow.exe` next to the launcher's own executable (not
the current working directory) and starts it with that directory as the child's
working directory. The launcher closes once the game has started. If `Wow.exe`
is missing or the process cannot be created, a message box explains why and the
launcher stays open.

## Modules

- `src/launcher/main.zig`: entry point, layout declaration, launch callback.
- `src/launcher/game.zig`: resolves and spawns `Wow.exe`.
- `src/clay/`: the layout engine: a Zig port of Clay v0.14's solver.
  - `types.zig`: element declarations (sizing, padding, alignment, floating).
  - `text.zig`: word measurement and line breaking.
  - `layout.zig`: the sizing and positioning passes.
  - `layout_test.zig`: unit tests with hand-computed expectations.
- `src/gui/`: scoped Win32 GUI layer, imported as `gui`.
  - `root.zig`: public surface (`createWindow`, `run`, `showMessage`, layout types).
  - `window.zig`: window class, message loop, layout replay, text measurement.
  - `layout.zig`: `gui`'s layout vocabulary; mirrors `clay` one to one.
  - `button.zig`: button widget; the `HWND` stays internal.
  - `label.zig`: static text widget; the `HWND` stays internal.
  - `geometry.zig`: `Rect`/`Size`/`Point` for widget placement.
- `src/lib/root.zig`: shared library, imported as `lib`.
- `src/lib/win32.zig`: `windows.h` via `@cImport`; the module links libc so the
  MinGW-w64 headers and CRT resolve.
- `installer/`: WiX source for `Arenacraft.msi` and its JScript validation
  action.
- `launcher.rc` / `AppIcon.ico`: PE resources; the icon is also used for the
  window class. `scripts/make_icon.py` regenerates the icon from
  `AppIcon.jpeg`.
- `scripts/changelog.sh`: release notes for the `launcher-release` workflow.

The GUI surface hides widget handles and takes layout in its own vocabulary, so
the engine behind it (`clay`) never appears in launcher code and can be swapped
without touching call sites. `gui/layout.zig` mirrors every engine option and
converts with exhaustive switches, so an engine change that is not mapped fails
to compile instead of silently doing nothing.

## I/O

`main` creates one `std.Io.Threaded` instance and passes it to the launch code.
Zig 0.16 has no `Io.Evented` backend on Windows, so `Threaded` is the
implementation: blocking syscalls run on its worker pool and
`io.async`/`io.concurrent` give non-blocking work. Long-running work (asset
downloads, patching) must not be awaited on the GUI thread; see
`ai-doc/launcher.md` for the intended pattern.

## Release

1. Create and push a tag named `launcher-x.y.z`.
2. Run the workflow on that tag, not on a branch:

   ```sh
   gh workflow run launcher-release.yml \
     --ref launcher-x.y.z -f tag=launcher-x.y.z
   ```

   The run ref becomes the provenance source ref, which is what `gh attestation
   verify --source-ref refs/tags/...` (printed in the release notes) checks.

The workflow builds `Arenacraft.exe` (`-Doptimize=ReleaseSafe`) and
`Arenacraft.msi`, generates notes from the commits touching `apps/launcher`
since the previous `launcher-*` tag, attests build provenance for both files,
and publishes them plus `SHA256SUMS`. Set `WINDOWS_SIGNING_PFX_BASE64` and
`WINDOWS_SIGNING_PFX_PASSWORD` to Authenticode-sign the artifacts.

The MSI installs the launcher into the World of Warcraft directory, so the
wizard asks for it. The folder page is pre-filled with, in order: the directory
this launcher was installed into last time (recorded under
`HKLM\SOFTWARE\Arenacraft\Launcher\InstallPath`), the client registered by
Blizzard (`HKLM\SOFTWARE\Blizzard Entertainment\World of Warcraft\InstallPath`),
or `%ProgramFiles%\World of Warcraft`. The install is refused unless `Wow.exe`
and `Data\common.MPQ` are present, and the chosen directory is recorded for the
next upgrade. Desktop and Start menu shortcuts are always installed; the MSI
has no feature page.
