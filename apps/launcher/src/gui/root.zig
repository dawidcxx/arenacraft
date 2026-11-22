//! Scoped Win32 GUI layer for the launcher.
//!
//! This is not a general Win32 binding: `lib.win32` is the raw `@cImport`
//! translation, and everything here is the subset of Win32 the launcher
//! actually uses, behind a small surface API.
//!
//! Two rules keep the abstraction replaceable:
//!
//! * Widget handles are opaque (`Button` hides its `HWND`), so a widget can
//!   gain extra state or move to custom painting without changing call sites.
//! * Widgets are positioned with `geometry.Rect`. A layout engine (yoga) only
//!   has to produce rects; it never leaks into launcher code.
//!
//! GUI entry points must stay on the thread that created the window. Future
//! I/O work runs off-thread and posts back to the window; see `ai-doc/launcher.md`.

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const widget = @import("button.zig");
const window = @import("window.zig");

pub const Rect = geometry.Rect;
pub const Size = geometry.Size;
pub const Point = geometry.Point;

pub const Button = widget.Button;
pub const ButtonOptions = widget.Options;
pub const Callback = widget.Callback;

pub const Window = window.Window;
pub const WindowOptions = window.Options;
pub const createWindow = window.create;
pub const run = window.run;

const allocator = std.heap.smp_allocator;

pub const MessageKind = enum {
    information,
    warning,
    /// `error` cannot be used as an enum literal (it is a keyword).
    critical,
};

/// Show a blocking message box. `title` and `text` are UTF-8.
pub fn showMessage(kind: MessageKind, title: []const u8, text: []const u8) void {
    const flags: c.UINT = switch (kind) {
        .information => c.MB_OK | c.MB_ICONINFORMATION,
        .warning => c.MB_OK | c.MB_ICONWARNING,
        .critical => c.MB_OK | c.MB_ICONERROR,
    };

    // Error text can embed paths read from the OS, which are WTF-8.
    const title_w = std.unicode.wtf8ToWtf16LeAllocZ(allocator, title) catch return;
    defer allocator.free(title_w);
    const text_w = std.unicode.wtf8ToWtf16LeAllocZ(allocator, text) catch return;
    defer allocator.free(text_w);

    _ = c.MessageBoxW(null, text_w.ptr, title_w.ptr, flags);
}
