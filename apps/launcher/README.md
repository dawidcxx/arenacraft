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

`windows.h` is not imported with `@cImport` (removed in Zig 0.17): `build.zig`
runs the official [translate-c](https://codeberg.org/ziglang/translate-c)
package to emit the `c` module from a small stub header. The translator is a
build tool, so it is compiled for the host (even when cross-compiling to
Windows) and cached after the first build.

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
natively: `zig build clay-test` needs no wine. The `realmlist` parser and file
selection are plain `std` too, so `zig build realmlist-test` also runs natively.
`zig build test` runs all three; `zig build lib-test`, `zig build clay-test`
and `zig build realmlist-test` run one at a time.

## Icon

`launcher.rc` embeds `AppIcon.ico` in the PE, so `launcher.exe` carries the
application icon for Explorer, the taskbar, Alt+Tab and shortcuts; `gui` loads
the same resource for the window class. `AppIcon.ico` is generated from
`AppIcon.jpeg` by `scripts/make_icon.py` (needs `ffmpeg` on `PATH`); regenerate
and commit it when the artwork changes.

## What it does

The window is a compact, phone-shaped column (360x520 by default), not a wide
dialog. Both views are declared with the layout vocabulary and re-wrapped on
resize; there are no hardcoded widget positions.

The **home** view shows **Arenacraft Launcher** with a tagline picked at random
from a small list at each start, then full-width **Launch Game**,
**Change Realm** and **Quit** buttons down the column, like a phone home
screen: one action per row, no side-by-side buttons. The realm the launch will
use sits in the bottom-left corner as small secondary text (`realm: host`, with
the host in semibold), so it stays visible without competing with the actions.
Nothing is written at startup, so a fresh installation gets its `realmlist.wtf`
when the game is launched or a realm changes.

Taglines live in the `taglines` constant in `src/launcher/app.zig`; add an
entry there. The one chosen for the current process is `App.tagline`.
They are stored uppercase because the native label has no letter-spacing, and
the choice is made once per process so switching views does not reshuffle the
line under the title.

The **realms** view (opened with **Change Realm**) fills the top with the
remembered realm list, then **Add**/**Remove** underneath, an editable
**Realm address** field with the selected realm's name, and full-width
**Apply** and **Cancel** buttons at the bottom. Apply commits every pending
edit (realm pick, address, additions and removals) and returns to the home
view; Cancel drops the draft and leaves the committed list untouched.

### Realm switcher

The list shows the realms the launcher knows: the built-in Arenacraft and
Warmane entries plus every logon host already present in the client's
`realmlist.wtf`, including commented-out ones. The switcher edits a **draft
copy** of the list: picking a row, typing an address, adding and removing only
change that copy, and `realmlist.wtf` is not touched until Apply.

**Apply** (or Enter) validates the address, writes the whole edited list and
returns to the home view. It re-points the selected realm at the typed address,
or appends the typed host as a new realm after **Add**. A host that is already
in the list is refused rather than duplicated. **Cancel** drops the draft and
returns home with the committed list exactly as it was, including any row picks
or removals made during the visit. **Remove** forgets the highlighted realm in
the draft: a custom host simply disappears from the history; a built-in realm
is recorded in `realmlist.wtf` as a `#hidden <host>` comment (the client ignores
comments) so it is not offered again on the next start. Typing a hidden host
brings it back. The last remaining realm cannot be removed, because the client
needs one to connect to.

A 3.3.5a client reads `Data/<locale>/realmlist.wtf`, so the launcher writes
every `Data/*/realmlist.wtf` it finds (and a root `realmlist.wtf` for pre-1.9
installations). With none present it creates the one for the locale in
`WTF/Config.wtf`, falling back to `Data/enUS/realmlist.wtf`. The active realm is
the first `set realmlist` line; every other remembered realm stays as a
`#set realmlist` line, which is also the format the launcher reads back at
startup. A typed realm joins the list on Apply, so the list grows with the player's
history. Invalid input (scheme, port, spaces, path separators) is rejected with
a message box.

The MSI installs into the World of Warcraft directory, so a game under
`Program Files` is owned by an administrator. If `realmlist.wtf` cannot be
written the launcher reports the path and error and leaves the game unstarted
rather than connecting to the wrong realm; move the client somewhere the user
can write to, or start the launcher as administrator.

**Launch Game** writes the current realm list, resolves `Wow.exe` next to the
launcher's own executable (not the current working directory), and starts it
with that directory as the child's working directory. The launcher closes once
the game has started. If `Wow.exe` is missing or the process cannot be created,
a message box explains why and the launcher stays open.

## Modules

- `src/launcher/main.zig`: entry point and window setup.
- `src/launcher/app.zig`: shared `App` state, view switching, taglines, message
  boxes.
- `src/launcher/home_view.zig`: home view and the launch/quit callbacks.
- `src/launcher/realms_view.zig`: realm switcher and its callbacks.
- `src/launcher/game.zig`: spawns `Wow.exe` from the installation directory.
- `src/launcher/install.zig`: resolves the game directory (the launcher's own).
- `src/launcher/realmlist.zig`: realm list parsing/rendering and the
  `realmlist.wtf` files of the installation; no Win32, tested natively.
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
  - `listbox.zig`: single-selection list; reports the picked row.
  - `edit.zig`: single-line text field; reports Enter (subclassed `EDIT`).
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
Zig 0.17 has no `Io.Evented` backend on Windows, so `Threaded` is the
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
wizard asks for it; the launcher writes `realmlist.wtf` there, so the directory
must be writable by the player. The folder page is pre-filled with, in order:
the directory this launcher was installed into last time (recorded under
`HKLM\SOFTWARE\Arenacraft\Launcher\InstallPath`), the client registered by
Blizzard (`HKLM\SOFTWARE\Blizzard Entertainment\World of Warcraft\InstallPath`),
or `%ProgramFiles%\World of Warcraft`. The install is refused unless `Wow.exe`
and `Data\common.MPQ` are present, and the chosen directory is recorded for the
next upgrade. Desktop and Start menu shortcuts are always installed; the MSI
has no feature page.
