# Launcher

Windows-only client launcher in `apps/launcher`. Read
`apps/launcher/README.md` for build/run/release commands; this note records the
design decisions that are easy to break.

## Layers

```
src/lib      raw Win32 bindings (@cImport windows.h)
src/gui      scoped GUI layer (windows, widgets, message boxes)
src/launcher app behaviour (launch Wow.exe)
```

- `lib` is only the translation of `windows.h`. No launcher semantics.
- `gui` is our own surface API, not a general Win32 wrapper. Widgets are two
  things on purpose:
  - an opaque handle (no public `HWND`), so the implementation can change;
  - a `button.Options`/`Window.addButton` call that only exposes what the
    launcher needs.
- `launcher` never calls `gui`/`lib` from a worker thread.

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

The current pass in `gui/window.zig` is a placeholder: one column, equal height
shares, 24px margin. Widgets are positioned through `Rect` and `Button.setRect`,
so replacing the placeholder with yoga means computing rects in `gui` and never
changing the launcher.

## Release

`launcher-release` builds `Arenacraft.exe` from an existing `launcher-x.y.z` tag
and packages `Arenacraft.msi` (WiX) with an embedded JScript check for `Wow.exe`
and `Data\common.MPQ`. The MSI build only runs on the Windows runner.
