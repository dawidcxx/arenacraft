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
- `launcher` never calls `gui`/`lib` from a worker thread (except
  `Dispatcher.post`), and never imports `clay`.
- Launcher code layout: `main.zig` (startup), `app.zig` (`App`, view switching,
  `alert`), `home_view.zig`, `realms_view.zig` (one file per view: its `build`
  and callbacks).
- Widget and container declarations are infallible: `beginLayout`/`endLayout`,
  `container`/`endContainer`, `spacer` and the `add*` methods assert their
  invariants in debug builds and panic when a native control cannot be created.
  Views therefore declare their tree without any `try`. `createWindow` still
  returns errors (reported with a message box before the window exists), as does
  `spawn` when a task cannot be started.
- Out of memory panics (`catch unreachable`) in `gui` and in the launcher app
  layer (`main`, `app`, views), so their error sets only carry real failures and
  call sites stay free of OOM branches. `realmlist.zig`, `game.zig`, `clay` and
  `lib` still propagate it. `gui/text.zig` treats invalid WTF-8 as a bug the
  same way.
- Widget callbacks are bound with `.bind(context_ptr, fn(*T) void)`
  (`gui/callback.zig`); never write `@ptrCast(@alignCast(...))` at call sites.
- Window internals keep one `controls` list of a `Control` union; adding a
  widget kind means a new union arm, not a new list/loop.

## GUI thread

Only the thread that created the window pumps messages and touches widgets.
`gui.run` blocks until the last window is destroyed. Close the window with
`Window.close`; `Window.deinit` destroys the native window (if still alive) and
frees the widget list.

Native widgets are identified by their child `HWND` (`WM_COMMAND`'s `lParam`).
Never fabricate an `HWND` from an integer through `@ptrFromInt`: the translated
handle types carry an alignment and Debug builds panic on unaligned values.
Compare `@intFromPtr` instead.

`windowProc` stores the `Window` in `WM_NCCREATE` but then forwards the message
to `DefWindowProcA` instead of returning `TRUE` itself. Returning early skips
the default proc, which is what applies the title passed to `CreateWindowExA`;
the window then has an empty caption even though the title was supplied.

Labels are native `STATIC` controls. Three things keep their text intact:

- `applyPlacements` invalidates every label after a layout pass. When a static
  grows, it repaints only the strip it gained, so the previous (slightly
  narrower) paint survives: changing "Arenacraft" to "New realm" showed just
  "New" until a resize repainted the whole control. `setLabelText` alone is not
  enough; the invalidation has to happen after the control has been resized.
- `.wrap = .none` creates the control with `SS_LEFTNOWORDWRAP`. A plain
  `SS_LEFT` static word-wraps, so a label whose measured width was a pixel
  short moved its last word to a second line that the one-line control clipped:
  "Arenacraft Launcher" rendered as "Arenacraft". The `wrap` option now selects
  the native style as well as the engine mode.
- Label containers reserve right and bottom slack. A `STATIC` clips at its
  right edge, and the measured advance does not include the last glyph's
  overhang; uppercase glyphs sit tighter against the advance edge, so caps
  labels get 8px of right slack and everything else 4px. Bottom slack covers
  descenders and rounding between the measuring DC and the control's DC.

## Views

The launcher has two views in one window: home and the realm switcher. A view
is just a layout declaration, so switching is `Window.clear` (destroy every
control, forget the tree) followed by recording the other view's tree. Widget
pointers in `App` are only valid until the next switch, which is why they are
optional and reset by `showView`.

Both views are single columns sized for a phone-shaped window: `main` asks for
360x520 and a 320x460 minimum, enforced by the window through
`WM_GETMINMAXINFO`. Actions are full-width buttons stacked vertically, one per
row; do not turn them back into a wide row of fixed-size buttons. The home
view keeps the realm as a small bottom-left footer (`realm:` plus the host,
one line, no wrapping), not as a headline above the actions.

- `gui/label.zig` defines the fonts (`body`, `small`, `small_bold`, `title`);
  `window.zig` creates one GDI font per enum value and derives the array size
  from the enum, so a new arm cannot index past it. The title size keeps
  "Arenacraft Launcher" on one line in the minimum window.
- Every control uses Tahoma: it ships with every supported Windows and is
  bundled by Wine, so the face that renders is the one that was measured.
  Request only normal and bold weights; GDI synthesises weights a face does not
  have, which looks chunky at the sizes used here. Avoid Segoe UI, which Wine
  may substitute.
- Home taglines are the `taglines` constant in `app.zig`, chosen once per
  process by `pickTagline(io)` into `App.tagline`. `buildUi` runs again on every view switch, so picking at
  build time would reshuffle the line when the realm switcher closes. Entries
  are stored uppercase: the native `STATIC` has no letter-spacing, and caps
  give the tagline a badge look below the title.

A callback may therefore destroy the control that raised it (e.g. `Change
Realm`). That is safe because `Window.dispatchCommand` returns
right after invoking the callback and never touches the freed widget again. Do
not add work after the callback call there.

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
- Tasks are structured: `Window.spawn(fn, args)` runs on the window's
  `Io.Group` (`concurrent`, so never inline on the GUI thread). `WM_DESTROY`
  cancels the group and waits for it, then drops callbacks still queued, so
  workers cannot outlive the window. Cancellation is cooperative: a task stops
  at its next `Io` call (`error.Canceled`); CPU-bound loops call
  `io.checkCancel()`.
- Results go back through `Window.dispatcher()`, a copyable `Dispatcher`:
  `dispatcher.post(.bind(ctx, fn))` runs `fn` on the GUI thread (private
  `WM_APP` message carrying a heap `Callback`). Tasks must only use `post`,
  never `SendMessage` (the cancel wait on the GUI thread would deadlock). The
  context must stay valid until the callback runs.

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
  computes and applies. Both are infallible, so a view pairs them with
  `window.beginLayout(); defer window.endLayout();`; an early return leaves the
  containers open and trips an assertion in debug builds instead of applying a
  half-built tree.
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
- The switcher edits a deep copy of the committed list (`List.clone`, held in
  `App.draft`). Picking a row, editing the address, Add and Remove only touch
  that copy. Apply validates, writes the copy and then adopts it
  (`App.commitDraft`); Cancel, leaving the view or closing the window drops the
  draft. This is what makes Cancel honest: nothing in `realmlist.wtf` has
  changed yet.
- Writes happen when Apply commits the draft, and again before launching. A
  failed write aborts the launch: connecting to the wrong realm is worse than
  not starting.
- A host is validated as `[A-Za-z0-9.-]+` (no scheme, port or path); anything
  else cannot go in that field anyway.
- Apply re-points the selected realm at the typed address (`List.rename`) and
  returns to the home view. It refuses a host that is already in the list
  instead of merging two rows. Renaming a built-in realm hides its old address
  exactly like Remove, or `ensurePresets` would resurrect it on the next start.
  A failed write (read-only install) keeps the switcher open with the draft
  intact for a retry or Cancel.
- Cancel drops the draft, including row picks, renames and removals made since
  the switcher opened; the committed list was never mutated.
- Add is a draft mode: it clears the field and the list highlight, and the next
  Apply appends the address as a new realm. Without that, a pre-filled field
  invites typing over the active host and saving the concatenation.
- Remove forgets the highlighted realm in the draft. Custom hosts just
  disappear from the history; a built-in realm would be appended again by
  `ensurePresets` on the next start, so committing a removal writes a
  `#hidden <host>` line. That is an ordinary comment for the client, and
  `parse`/`Client.load` merge it back so the preset stays gone. Typing the host
  again clears the marker. The last realm cannot be removed: the client needs
  one.
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
- `gui/edit.zig` wraps an `EDIT`. The address field mirrors the selected realm
  and Apply commits it; Add clears the field and the highlight first. Enter is
  caught by a window procedure subclass (`SetWindowLongPtrW(GWLP_WNDPROC)`) that
  forwards everything but `VK_RETURN` to `CallWindowProcW`.
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
