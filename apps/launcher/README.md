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

The test binaries are Windows PE as well, so they need the same executor.
`zig build lib-test` runs the lib tests on their own.

## What it does

The window shows a single **Launch Game** button. It resolves `Wow.exe` next to
the launcher's own executable (not the current working directory) and starts it
with that directory as the child's working directory. The launcher closes once
the game has started. If `Wow.exe` is missing or the process cannot be created,
a message box explains why and the launcher stays open.

## Modules

- `src/launcher/main.zig`: entry point, window setup, launch callback.
- `src/launcher/game.zig`: resolves and spawns `Wow.exe`.
- `src/gui/`: scoped Win32 GUI layer, imported as `gui`.
  - `root.zig`: public surface (`createWindow`, `run`, `showMessage`, types).
  - `window.zig`: window class, message loop, placeholder layout pass.
  - `button.zig`: button widget; the `HWND` stays internal.
  - `geometry.zig`: `Rect`/`Size`/`Point`, the seam for a layout engine.
- `src/lib/root.zig`: shared library, imported as `lib`.
- `src/lib/win32.zig`: `windows.h` via `@cImport`; the module links libc so the
  MinGW-w64 headers and CRT resolve.
- `installer/`: WiX source for `Arenacraft.msi` and its JScript validation
  action.
- `scripts/changelog.sh`: release notes for the `launcher-release` workflow.

The GUI surface deliberately hides widget handles and positions widgets with
`Rect`, so a layout engine (yoga) can take over layout, and widgets can gain
state or custom painting, without changing launcher code.

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

The MSI defaults to the World of Warcraft directory registered by Blizzard,
lets the user pick another one, and refuses to install unless `Wow.exe` and
`Data\common.MPQ` are present. Desktop and Start menu shortcuts are optional
features.
