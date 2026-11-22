//! Push button widget.
//!
//! The control is a native Win32 `BUTTON` today. `Button` is an opaque handle:
//! launcher code never sees the `HWND`, so the backing implementation can move
//! to a custom-painted widget (positioned by the layout engine, carrying
//! whatever extra state it needs) without touching call sites.

const lib = @import("lib");
const c = lib.win32.c;

const geometry = @import("geometry.zig");
const layout = @import("layout.zig");

/// Called on the GUI thread when the button is pressed.
pub const Callback = struct {
    function: *const fn (context: ?*anyopaque) void,
    context: ?*anyopaque = null,
};

pub const Options = struct {
    text: []const u8,
    onClick: Callback,
    sizing: layout.Sizing = .{},
    padding: layout.Padding = .symmetric(16, 8),
};

pub const Button = struct {
    hwnd: c.HWND,
    callback: Callback,
    sizing: layout.Sizing,
    padding: layout.Padding,
    /// Measured label size, so `.fit` never shrinks below the control's text.
    text_size: geometry.Size = .{},
    rect: geometry.Rect = .{},

    /// Record the bounds computed by the layout pass. The window moves all
    /// controls in one `DeferWindowPos` batch afterwards.
    pub fn setRect(self: *Button, rect: geometry.Rect) void {
        self.rect = rect;
    }
};
