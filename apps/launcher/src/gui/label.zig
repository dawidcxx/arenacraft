//! Static text widget.
//!
//! Backed by a native `STATIC` control, like `Button` is backed by a native
//! `BUTTON`. The layout engine measures the same string with the same font, so
//! the control is sized exactly once per layout pass.
//!
//! Wrapped labels rely on the `STATIC` control's own word wrapping (`SS_LEFT`);
//! the engine's line breaking exists to compute the height, not to draw text.

const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");

pub const Font = enum(u32) {
    body = 0,
    title = 1,
};

pub const Options = struct {
    text: []const u8,
    font: Font = .body,
    wrap: layout.WrapMode = .words,
    sizing: layout.Sizing = .{},
};

pub const Label = struct {
    hwnd: c.HWND,
    /// Owned copy of the text, replayed on every layout pass.
    text: [:0]const u8,
    font: Font,
    wrap: layout.WrapMode,
    sizing: layout.Sizing,
    rect: geometry.Rect = .{},

    /// Record the bounds computed by the layout pass.
    pub fn setRect(self: *Label, rect: geometry.Rect) void {
        self.rect = rect;
    }
};
