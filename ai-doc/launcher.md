# Launcher

Windows-only client launcher in `apps/launcher`. Read
`apps/launcher/README.md` for build/run/release commands; this note records the
design decisions that are easy to break.

## Layers

```
src/lib      raw Win32 bindings (@cImport windows.h)
src/clay     layout engine (Zig port of Clay's solver; no Win32)
src/gui      scoped GUI layer (windows, widgets, layout replay, message boxes)
src/launcher app behaviour (launch Wow.exe)
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
  `addLabel` append to `Window.plan`; `endLayout` computes and applies.
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
