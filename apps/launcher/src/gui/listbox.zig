//! Single-selection list widget.
//!
//! Backed by a native `LISTBOX`, like `Button` is backed by a native `BUTTON`.
//! The launcher uses it for the remembered realms: `onSelect` fires when the
//! player picks a row and the application copies that row into the address
//! field. `ListBox` is opaque so the control can move to custom painting
//! without touching call sites.

const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");
const Callback = @import("callback.zig").Callback;

pub const Options = struct {
    /// Initial rows, in display order. The native control copies the strings.
    items: []const []const u8,
    /// Index of the initially highlighted row, or none.
    selected: ?usize = null,
    onSelect: Callback,
    sizing: layout.Sizing = .{
        .width = .{ .grow = .{} },
        .height = .{ .grow = .{} },
    },
};

pub const ListBox = struct {
    hwnd: c.HWND,
    callback: Callback,
    sizing: layout.Sizing,
    rect: geometry.Rect = .{},

    /// Record the bounds computed by the layout pass.
    pub fn setRect(self: *ListBox, rect: geometry.Rect) void {
        self.rect = rect;
    }
};
