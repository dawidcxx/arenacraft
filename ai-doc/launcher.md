# Launcher

Windows-only client launcher in `apps/launcher`. Read
`apps/launcher/README.md` for build/run/release commands; this note records the
design decisions that are easy to break.

## Layers

```
src/lib      raw Win32 bindings (@cImport windows.h)
src/clay     layout engine (Zig port of Clay's solver; no Win32)
src/gui      scoped GUI layer (windows, widgets, layout replay, message boxes)
src/launcher app behaviour (realm selection, launch Wow.exe)
```

- `lib` is only the translation of `windows.h`. No launcher semantics.
- `clay` is a port, not a binding: the layout half of Clay v0.14 in plain Zig,
  with no libc, no Win32 and no global state, so it builds and tests on the host.
- `gui` is our own surface API, not a general Win32 wrapper. Widgets are two
  things on purpose:
  - an opaque handle (no public `HWND`), so the implementation can change;
  - a `button.Options`/`Window.addButton` call that only exposes what the
    launcher needs.
- `launcher` never calls `gui`/`lib` from a worker thread, and never imports
  `clay`.

## GUI thread

Only the thread that created the window pumps messages and touches widgets.
`gui.run` blocks until the last window is destroyed. Close the window with
`Window.close`; `Window.deinit` destroys the native window (if still alive) and
frees the widget list.

Native widgets are identified by their child `HWND` (`WM_COMMAND`'s `lParam`).
Never fabricate an `HWND` from an integer through `@ptrFromInt`: the translated
handle types carry an alignment and Debug builds panic on unaligned values.
Compare `@intFromPtr` instead.

## Views

The launcher has two views in one window: home and the realm switcher. A view
is just a layout declaration, so switching is `Window.clear` (destroy every
control, forget the tree) followed by recording the other view's tree. Widget
pointers in `App` are only valid until the next switch, which is why they are
optional and reset by `showView`.

A callback may therefore destroy the control that raised it (e.g. `Change
Realm`). That is safe because `windowProc` breaks out of the `WM_COMMAND` loop
right after invoking the callback and never touches the freed widget again. Do
not add work after the callback call in that loop.

## I/O

One `std.Io.Threaded` instance is created in `main` and passed to callbacks.
Zig 0.16 has no `Io.Evented`/fiber backend for Windows, so `Threaded` is the
implementation: blocking syscalls run on its worker pool.

Rules for future work (downloads, patching):

- Never `await` a long-running `Io` operation on the GUI thread; it blocks the
  message loop and freezes the window.
- Run the blocking operation with `io.concurrent(...)`/`io.async(...)`; collect
  the result in the worker, then `PostMessage` a custom `WM_APP+n` message to
  the window. The GUI thread applies the result and updates widgets.
- Anything the `gui` layer adds for this should keep the marshalling inside
  `gui` (a `Window.post` style primitive), not in launcher code.

## Layout

`src/clay/` is a Zig port of Clay v0.14's layout solver: the sizing pass, text
wrapping, height propagation and positioning. Only the layout half exists;
colors, corner radius, borders, clipping/scrolling, transitions, pointer/hover
state and render-command generation are not ported. The deviations from upstream
are listed at the top of `src/clay/layout.zig`; the ones that matter:

- no clip/scroll containers, so a container never ignores its children's minimum
  sizes;
- floating elements attach only to their parent or to the layout root (attaching
  by element id needs Clay's id hash map);
- roots are positioned in declaration order, without z-index sorting;
- a `percent` axis is not re-clamped after height propagation. Clay reads
  min/max through the union's percent member there, which is undefined behaviour
  in C.

The engine is immediate-mode but a Win32 window only hears about resizes through
`WM_SIZE`, so `gui` records the tree and replays it:

- `Window.beginLayout` starts recording. `container`/`endContainer`/`addButton`/
  `addLabel`/`addListBox`/`addEdit` append to `Window.plan`; `endLayout`
  computes and applies.
- Every layout pass (`WM_SIZE`, and the initial `endLayout`) resets the engine's
  per-frame arena, replays `plan`, computes, and moves all controls in a single
  `DeferWindowPos` batch. There is no per-widget `MoveWindow` in the common case.
- `gui/layout.zig` mirrors the engine's declarations (`Container`, `SizingAxis`,
  `Padding`, `Alignment`, `WrapMode`) and converts with exhaustive switches.
  Launcher code uses only these names; adding an engine option without mapping
  it fails to compile.
- Text is measured by `gui` with the control's font (`GetTextExtentPoint32W` on
  a screen DC). Clay does the line breaking, so a `STATIC`'s height matches its
  own word wrapping. Labels are wrapped in a fit container so they accept the
  same `sizing` options as other widgets; buttons get a `.fit` minimum of the
  measured label plus their padding.

`zig build clay-test` runs the layout tests natively (they do not need wine).
`layout_test.zig` covers fit/grow/percent/fixed, shrink with minimums,
alignment, wrapping modes, floating and a launcher-shaped tree; the expected
numbers were cross-checked against upstream Clay compiled from `clay.h` v0.14.

Testing note: without a window manager, resizing the X window does not reach
Wine's Win32 window, so no `WM_SIZE` is delivered. Drive resizes with
`SetWindowPos` from a helper process instead.

## Realm switcher

The realm lives in the client's `realmlist.wtf`, not in our own config:
`Data/<locale>/realmlist.wtf` in 3.3.5a (a root `realmlist.wtf` for pre-1.9
installs). The file is both the setting and its history:

```
set realmlist logon.arenacraft.fun
#set realmlist logon.warmane.com
#set realmlist 62.83.16.59
```

`realmlist.zig` parses every `set realmlist <host>` line (`#` comments are
remembered too), keeps the first uncommented one active, and renders the active
host first with the rest commented out. Non-realm lines are ignored, and a
realmlist file is rewritten with realm lines only: the launcher owns that
state.

- Presets are hardcoded in `realmlist.zig` (Arenacraft, Warmane) and appended
  when the file history lacks them. Do not fetch the list from the API: the
  switcher has to work offline, before the player ever logs in.
- `Client` resolves every realmlist file once at startup by iterating `Data/*`
  and checking `realmlist.wtf`. With none present it picks `SET locale "xxYY"`
  from `WTF/Config.wtf`, else the first locale-shaped `Data` directory, else
  enUS; `write` creates the directories and the file.
- Writes happen when a realm row is picked, when Enter is pressed in the address
  field, and again before launching, so a host typed but not submitted still
  reaches the client. A failed write aborts the launch: connecting to the wrong
  realm is worse than not starting.
- A host is validated as `[A-Za-z0-9.-]+` (no scheme, port or path); anything
  else cannot go in that field anyway.
- Remove forgets the highlighted realm and rewrites the file. Custom hosts just
  disappear from the history; a built-in realm would be appended again by
  `ensurePresets` on the next start, so removing one writes a `#hidden <host>`
  line. That is an ordinary comment for the client, and `parse`/`Client.load`
  merge it back so the preset stays gone. Typing the host again clears the
  marker. The last realm cannot be removed: the client needs one.
- The file is rewritten unconditionally (line endings become `\n`), which also
  normalizes hand-edited duplicates away.
- The MSI is per-machine and may install into Program Files; a non-elevated
  launcher then cannot write `Data/<locale>/realmlist.wtf`. The write error
  tells the player to move the client or run as administrator instead of
  silently launching against the old realm.

The picker is deliberately *not* a combo box: a native `COMBOBOX` mixes an edit
field, a popup list and their notifications in one control, and every piece of
it has to be understood before the launcher behaves. Two plain controls are
easier to reason about and to drive:

- `gui/listbox.zig` wraps a `LISTBOX` (`LBS_NOTIFY`) and calls `onSelect` on
  `LBN_SELCHANGE`; `Window.listSelection` reads the picked row.
- `gui/edit.zig` wraps an `EDIT`. The address is authoritative for launching, so
  `Window.editText` is read right before writing; Enter is caught by a window
  procedure subclass (`SetWindowLongPtrW(GWLP_WNDPROC)`) that forwards everything
  but `VK_RETURN` to `CallWindowProcW`.
- `Window.setListItems` repopulates after a typed host joins the list or a realm
  is removed. `LB_SETCURSEL` does not notify the parent, so reselecting cannot
  re-enter the selection callback. Do not call it from inside `LBN_SELCHANGE`:
  resetting the rows while the control is still handling the click leaves the
  native selection out of sync with the active realm. Inside the notification
  use `Window.setListSelection`, which only moves the highlight.
- Both controls copy their strings, so call sites keep ownership of the item
  list and may free it after adding rows.

`Window.setLabelText` replaces a label and re-runs the layout, because labels
are sized to their text and the active realm label changes length.

## Icon

`launcher.rc` embeds `AppIcon.ico` in the PE; the lowest icon ID is the
application icon, which is why Explorer, the taskbar and shortcuts use it and
not just the window. `gui/window.zig` loads the same ID for the window class
(`hIcon`/`hIconSm`). `AppIcon.ico` is generated from `AppIcon.jpeg` by
`scripts/make_icon.py` (uses `ffmpeg`); regenerate and commit it when the
artwork changes. The `.ico` must be committed as a binary — `.gitattributes`
already marks it so, otherwise CRLF normalization corrupts the PNG-compressed
256 px entry (this happened to the old updater app's icon).

`LoadImageA` returns an untyped `HANDLE`, but the translated `HICON` is a
pointer to a dummy `int`-bearing struct, so it carries alignment 4 while handle
values are not required to be aligned (Wine hands out odd ones, and Debug/
ReleaseSafe panics with "incorrect alignment"). `handleToIcon` reinterprets the
bits with `@memcpy`; a handle is never dereferenced, so its alignment is
irrelevant.

## Installer

`installer/Arenacraft.wxs` is the WiX source (plus `wowcheck.js`, the embedded
JScript directory check, and `License.rtf`); the release workflow builds it on
the Windows runner. Don't try to validate it by running the `wix` dotnet tool
locally on Linux: it starts (with a "only supports Windows" warning) but rejects
every `Directory/@Name` with `WIX0389`, binds no MSI, and needs Windows'
`msi.dll` for file versions. Use the workflow.

The install directory *is* the World of Warcraft directory: `Arenacraft.exe`
lives next to `Wow.exe` and resolves the game relative to itself. So the wizard
has to let the user pick it.

- The dialog set is `WixUI_InstallDir`, which has a folder page.
  `WixUI_Mondo` was used before: its only directory picker is
  Customise → Browse, so in practice users could not choose where to install.
  The trade-off is that `InstallDir` has no feature page, so the shortcut
  features are always installed (they stay features so `ADDLOCAL`/`REMOVE`
  still work).
- Pre-selected directory, in order: our own
  `HKLM\SOFTWARE\Arenacraft\Launcher\InstallPath` (written on install) →
  Blizzard's `HKLM\SOFTWARE\Blizzard Entertainment\World of Warcraft\InstallPath`
  → `ProgramFiles6432Folder\World of Warcraft`. Ours comes first so an upgrade
  lands where the user actually installed instead of wherever the Blizzard key
  or Program Files points.
- The two `SetProperty` defaults have mutually exclusive conditions (each
  excludes all earlier sources) rather than relying on the order WiX emits
  them in. `NOT INSTALLDIR` keeps a command-line `INSTALLDIR` and a reinstall of
  the same ProductCode untouched.
- Registry view: the package is x64, so our key is written to the 64-bit view and
  read with `Bitness="always64"`. The 3.3.5a client is 32-bit, so Blizzard's key
  is read with `Bitness="always32"`.
- `ValidateWowDir` runs `After="CostFinalize"` in the *execute* sequence: after
  the folder page has set `INSTALLDIR`, but before `RemoveExistingProducts`
  (upgrades default to `afterInstallValidate`), so a rejected directory leaves
  the previous install untouched. It cannot run in the UI sequence, where the
  page has not been shown yet. A rejected directory therefore fails at install
  time rather than on the folder page.

## Release

`launcher-release` builds `Arenacraft.exe` from an existing `launcher-x.y.z` tag
and packages `Arenacraft.msi` (WiX) with an embedded JScript check for `Wow.exe`
and `Data\common.MPQ`. The MSI build only runs on the Windows runner.

- Build with ReleaseSafe: the launcher has no performance-critical path, and
  safety checks make a bad build fail loudly instead of silently.
- Run the workflow on the tag ref
  (`gh workflow run launcher-release.yml --ref launcher-x.y.z -f tag=launcher-x.y.z`).
  The provenance attestation records `github.ref`; running on a branch makes
  `--source-ref refs/tags/...` verification fail even though the tag was
  checked out for the build.
- Provenance is mandatory: `actions/attest-build-provenance` is not
  `continue-on-error`, so a release cannot claim a verification path it does
  not have.
- Tags are not signed yet; a GitHub "Verified" badge requires a GPG/SSH-signed
  tag from a key registered on the account, which a workflow cannot supply.
