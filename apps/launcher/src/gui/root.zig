//! Scoped Win32 GUI layer for the launcher.
//!
//! This is not a general Win32 binding: `lib.win32` is the raw `@cImport`
//! translation, and everything here is the subset of Win32 the launcher
//! actually uses, behind a small surface API.
//!
//! Three rules keep the abstraction replaceable:
//!
//! * Widget handles are opaque (`Button`, `Label` hide their `HWND`), so a
//!   widget can gain extra state or move to custom painting without changing
//!   call sites.
//! * Layout is declared in `gui`'s own vocabulary (`Container`, `Sizing`,
//!   `Padding`, ...) and never in engine types. `layout.zig` mirrors the `clay`
//!   module one-to-one and converts, so launcher code has no dependency on the
//!   layout engine and it can be swapped without touching call sites.
//! * Widgets are positioned with whole-pixel `geometry.Rect`s produced by the
//!   layout pass; nothing in the public surface knows how they are computed.
//!
//! GUI entry points must stay on the thread that created the window. Future
//! I/O work runs off-thread and posts back to the window; see `ai-doc/launcher.md`.

const std = @import("std");
const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");
const widget = @import("button.zig");
const label = @import("label.zig");
const window = @import("window.zig");

pub const Rect = geometry.Rect;
pub const Size = geometry.Size;
pub const Point = geometry.Point;

pub const Button = widget.Button;
pub const ButtonOptions = widget.Options;
pub const Callback = widget.Callback;

pub const Label = label.Label;
pub const LabelOptions = label.Options;
pub const Font = label.Font;

pub const Container = layout.Container;
pub const Sizing = layout.Sizing;
pub const SizingAxis = layout.SizingAxis;
pub const MinMax = layout.MinMax;
pub const Padding = layout.Padding;
pub const Direction = layout.Direction;
pub const AlignX = layout.AlignX;
pub const AlignY = layout.AlignY;
pub const Alignment = layout.Alignment;
pub const WrapMode = layout.WrapMode;

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
