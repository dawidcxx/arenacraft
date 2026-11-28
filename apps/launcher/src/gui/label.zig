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
    /// Default control text.
    body = 0,
    /// Secondary detail text, used for the realm footer and the tagline.
    small = 1,
    /// Emphasised secondary text (the realm host in the footer).
    small_bold = 2,
    /// Window and view titles.
    title = 3,
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
