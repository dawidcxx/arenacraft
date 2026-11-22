//! Push button widget.
//!
//! The control is a native Win32 `BUTTON` today. `Button` is an opaque handle:
//! launcher code never sees the `HWND`, so the backing implementation can move
//! to a custom-painted widget (positioned by the layout engine, carrying
//! whatever extra state it needs) without touching call sites.

const lib = @import("lib");
const c = lib.win32.c;

const Rect = @import("geometry.zig").Rect;

/// Called on the GUI thread when the button is pressed.
pub const Callback = struct {
    function: *const fn (context: ?*anyopaque) void,
    context: ?*anyopaque = null,
};

pub const Options = struct {
    text: []const u8,
    onClick: Callback,
};

pub const Button = struct {
    hwnd: c.HWND,
    /// Last bounds handed to the widget. Hit-testing and the layout engine will
    /// read this once widgets stop being real child windows.
    rect: Rect = .{},
    callback: Callback,

    /// Move/resize the native control. Called by the window's layout pass.
    pub fn setRect(self: *Button, rect: Rect) void {
        self.rect = rect;
        _ = c.MoveWindow(self.hwnd, rect.x, rect.y, rect.width, rect.height, c.TRUE);
    }
};
